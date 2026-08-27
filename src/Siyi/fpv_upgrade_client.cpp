#include "fpv_upgrade_client.h"
#include "ftp_upload.h"

#include <chrono>
#include <thread>
#include <cstring>

namespace unircsdk {

// C++14 still needs an out-of-line definition for odr-used static constexpr
// members (inline variables are C++17).
constexpr int FpvUpgradeClient::REBOOT_STABLE_MS;
constexpr int FpvUpgradeClient::FTP_TIMEOUT_MS;
constexpr int FpvUpgradeClient::FTP_TIMEOUT_SLOW_MS;

namespace {
constexpr size_t kMaxDatagram = 2048;

void sleepMs(int ms) {
    if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
} // namespace

const char* FpvUpgradeClient::resultName(Result r) {
    switch (r) {
        case Result::Ok:                       return "OK";
        case Result::PackRepeat:               return "PACK_REPEAT";
        case Result::ErrPackNum:               return "ERR_PACK_NUM";
        case Result::ErrFile:                  return "ERR_FILE";
        case Result::ErrCheckSum:              return "ERR_CHECK_SUM";
        case Result::ErrWriteFileSize:         return "ERR_WRITE_FILE_SIZE";
        case Result::ErrFileSize:              return "ERR_FILE_SIZE";
        case Result::ErrConfigVersionMismatch: return "ERR_CONFIG_VERSION_MISMATCH";
        case Result::Timeout:                  return "TIMEOUT";
        case Result::BadReply:                 return "BAD_REPLY";
        case Result::NotImplemented:           return "NOT_IMPLEMENTED";
    }
    return "?";
}

// Faithful port of biz.siyi.n5.a.h(int) / l5.j.i(int): a chain of explicit
// comparisons where anything unlisted falls through to OK. Reproduced as-is
// (rather than defaulting unknown codes to an error) so behaviour matches the
// device's own client.
FpvUpgradeClient::Result FpvUpgradeClient::fromWire(int8_t code) {
    switch (code) {
        case -15: return Result::ErrConfigVersionMismatch;
        case -13: return Result::ErrFileSize;
        case -10: return Result::ErrWriteFileSize;
        case  -7: return Result::ErrCheckSum;
        case  -3: return Result::ErrFile;
        case  -1: return Result::ErrPackNum;
        case   3: return Result::PackRepeat;
        default:  return Result::Ok;
    }
}

bool FpvUpgradeClient::open(const std::string& ip, uint16_t port) {
    seq_ = 0;
    return port_.open(ip, port);
}

void FpvUpgradeClient::close() {
    port_.close();
}

bool FpvUpgradeClient::transact(uint8_t subCmd, const std::vector<uint8_t>& payload,
                                FpvFrame& reply, int timeoutMs) {
    if (!port_.isOpen()) return false;

    std::vector<uint8_t> frame = FpvFrame::build(
        FpvFrame::DEV_PARAM_AD_TOOL,   // src = us
        FpvFrame::DEV_TRANSMISSION,    // dst = FPV module
        FpvFrame::CMD_ID_UPGRADE,
        subCmd,
        seq_++,
        payload,
        /*needAck=*/true);

    if (!port_.send(frame.data(), frame.size())) return false;

    // Drain until a frame that actually belongs to this exchange shows up or the
    // deadline passes: an unsolicited/stale datagram must not be mistaken for
    // our reply, and must not consume the whole timeout budget either.
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMs);
    uint8_t buf[kMaxDatagram];

    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return false;
        const int remainMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());

        const int n = port_.recv(buf, sizeof(buf), remainMs);
        if (n <= 0) return false;   // 0 = timeout, -1 = error

        FpvFrame f;
        if (!FpvFrame::parse(buf, static_cast<size_t>(n), f)) continue;

        // Reply carries SRC/DST swapped w.r.t. our request.
        if (f.src != FpvFrame::DEV_TRANSMISSION)  continue;
        if (f.dst != FpvFrame::DEV_PARAM_AD_TOOL) continue;
        if (f.cmdId != FpvFrame::CMD_ID_UPGRADE)  continue;
        if (f.subCmd != subCmd)                   continue;

        reply = f;
        return true;
    }
}

bool FpvUpgradeClient::queryVersion(std::string& out, int timeoutMs) {
    FpvFrame reply;
    if (!transact(SUB_VERSION, {}, reply, timeoutMs)) return false;
    out.assign(reply.data.begin(), reply.data.end());
    return true;
}

FpvUpgradeClient::Result FpvUpgradeClient::sendStart(const std::string& filename,
                                                     uint16_t& startAck,
                                                     int retries, int retryMs) {
    if (!port_.isOpen()) return Result::BadReply;

    const std::vector<uint8_t> payload(filename.begin(), filename.end());

    for (int attempt = 0; attempt < retries; ++attempt) {
        if (attempt > 0) sleepMs(retryMs);

        FpvFrame reply;
        if (!transact(SUB_START, payload, reply, DEFAULT_REPLY_WAIT_MS)) continue;

        // Captured reply: 01 00 7D  => ok=1, u16=0x7D00. The u16's meaning is
        // unknown and unused on the FTP path; pass it through untouched.
        if (reply.data.size() < 3) return Result::BadReply;
        if (reply.data[0] != 1)    return Result::ErrFile;

        startAck = static_cast<uint16_t>(reply.data[1] | (reply.data[2] << 8));
        return Result::Ok;
    }
    return Result::Timeout;
}

FpvUpgradeClient::Result FpvUpgradeClient::sendConfirm(int timeoutMs) {
    if (!port_.isOpen()) return Result::BadReply;

    FpvFrame reply;
    if (!transact(SUB_CONFIRM, {}, reply, timeoutMs)) return Result::Timeout;
    if (reply.data.empty()) return Result::BadReply;

    return fromWire(static_cast<int8_t>(reply.data[0]));
}

bool FpvUpgradeClient::waitForReboot(std::string& versionOut,
                                     int overallTimeoutMs, int pollMs) {
    // Observed on hardware: after CONFIRM the module KEEPS ANSWERING for a while
    // (still running the old firmware) before it actually drops off to flash.
    // Returning on the first successful probe therefore reports "update
    // complete" while the module is still writing itself — a genuinely
    // dangerous lie, since the user may then power off or fly.
    //
    // So: wait for it to go away, then wait for it to come back AND STAY back.
    // It reboots twice, so a single reply is not proof that it has finished.
    const auto start    = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::milliseconds(overallTimeoutMs);
    const auto now      = [] { return std::chrono::steady_clock::now(); };

    // Phase 1 — wait for the module to drop off.
    bool sawDown = false;
    while (now() < deadline) {
        std::string v;
        if (!queryVersion(v, 1000) || v.empty()) {
            sawDown = true;
            break;
        }
        sleepMs(pollMs);
    }
    if (!sawDown) {
        // Never went down within the budget. Something is off — do not claim
        // success, the caller can re-probe.
        return false;
    }

    // Phase 2 — wait for it to answer again, and keep answering for a settle
    // window long enough to cover the second reboot.
    std::chrono::steady_clock::time_point stableSince{};
    while (now() < deadline) {
        std::string v;
        if (queryVersion(v, 1000) && !v.empty()) {
            if (stableSince == std::chrono::steady_clock::time_point{}) {
                stableSince = now();
            }
            versionOut = v;
            if (now() - stableSince >= std::chrono::milliseconds(REBOOT_STABLE_MS)) {
                return true;
            }
        } else {
            // Went away again: that is the second reboot. Start the settle
            // window over rather than treating the earlier reply as final.
            stableSince = {};
        }
        sleepMs(pollMs);
    }
    return false;
}

const char* FpvUpgradeClient::phaseName(Phase p) {
    switch (p) {
        case Phase::Announce: return "Announce";
        case Phase::Upload:   return "Upload";
        case Phase::Verify:   return "Verify";
        case Phase::Commit:   return "Commit";
        case Phase::Reboot:   return "Reboot";
    }
    return "?";
}

FpvUpgradeClient::Result FpvUpgradeClient::upgrade(const std::string& binPath,
                                                   ProgressFn progress,
                                                   std::string* newVersionOut,
                                                   uint16_t ftpPort,
                                                   int ftpTimeoutMs) {
    if (!port_.isOpen()) return Result::BadReply;

    // basename. SIYI's own files end in ".zip1" — that is the real extension,
    // so the name goes through verbatim.
    std::string name = binPath;
    const size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);

    const auto report = [&progress](Phase p, int pct) { if (progress) progress(p, pct); };

    // 1. announce the filename
    report(Phase::Announce, -1);
    uint16_t ack = 0;
    Result r = sendStart(name, ack);
    if (r != Result::Ok) return r;

    // 2. the actual firmware bytes, over FTP
    report(Phase::Upload, 0);
    uint64_t localSize = 0;
    FtpResult fr = ftpStoreFile(
        port_.peerIp(), ftpPort, name, binPath,
        [&](uint64_t sent, uint64_t total) {
            localSize = total;
            if (total) report(Phase::Upload, static_cast<int>((sent * 100) / total));
        },
        "anonymous", "anonymous", ftpTimeoutMs);
    if (!fr.ok) return Result::ErrFile;

    // 3. integrity gate — prove the whole file landed before letting the module
    //    flash it. A short upload committed here is an unbootable module.
    report(Phase::Verify, -1);
    uint64_t remoteSize = 0;
    FtpResult sr = ftpFileSize(port_.peerIp(), ftpPort, name, remoteSize,
                               "anonymous", "anonymous", ftpTimeoutMs);
    if (!sr.ok) return Result::ErrFile;
    if (localSize && remoteSize != localSize) return Result::ErrFileSize;

    // 4. flash + reboot. Everything past here is irreversible.
    report(Phase::Commit, -1);
    r = sendConfirm();
    if (r != Result::Ok) return r;

    // 5. wait for it to come back. NOTE: the module answers once, then reboots
    //    AGAIN (~65s) before it settles — see FIRMWARE_UPGRADE_PROTOCOL.md.
    report(Phase::Reboot, -1);
    if (newVersionOut) {
        std::string v;
        if (waitForReboot(v)) *newVersionOut = v;
    }
    return Result::Ok;
}

// ---------------------------------------------------------------------------
// The other file-transfer mode (h4.a.UDP): chunk the payload over SUBCMD 3 and
// commit with SUBCMD 4 (length + MD5-as-hex-string). This hardware is configured
// for FTP and was never observed emitting either frame, so these layouts remain
// decompile-only guesses and are intentionally inert.
// ---------------------------------------------------------------------------

FpvUpgradeClient::Result FpvUpgradeClient::sendChunk(uint16_t, const uint8_t*, size_t, uint16_t&) {
    return Result::NotImplemented;
}

FpvUpgradeClient::Result FpvUpgradeClient::sendFinish(uint32_t, const std::string&) {
    return Result::NotImplemented;
}

// ---------------------------------------------------------------------------

bool FpvUpgradeClient::selfTest() {
    // The exact bytes UniGCS put on the wire (logcat "WriteTask", 2026-08-13),
    // sending SUBCMD 2 for this firmware file at SEQ 0. Both CRCs in this frame
    // were independently recomputed and matched.
    static const uint8_t kGolden[] = {
        0xAA, 0x09, 0x03, 0x28, 0x00, 0x4E, 0x00, 0x00, 0xD0, 0x1E, 0x6F, 0x02,
        'C','X','6','6','5','3','C','-','N','_','A','5','.','1','.','2','.','6',
        '-','s','v','n','7','5','-','2','0','2','6','-','0','6','-','0','8','.',
        'z','i','p','1',
        0x16, 0x09
    };
    const size_t kGoldenLen = sizeof(kGolden);

    // Rebuild it. The payload is taken verbatim from the capture, trailing 0x31
    // ('1') included, precisely because sendStart() does NOT synthesise that
    // byte — this pins the framing, not the filename policy.
    const std::vector<uint8_t> payload(kGolden + FpvFrame::HEADER_LEN,
                                       kGolden + kGoldenLen - 2);
    std::vector<uint8_t> built = FpvFrame::build(
        FpvFrame::DEV_PARAM_AD_TOOL, FpvFrame::DEV_TRANSMISSION,
        FpvFrame::CMD_ID_UPGRADE, SUB_START, /*seq=*/0, payload, /*needAck=*/true);

    if (built.size() != kGoldenLen) return false;
    if (std::memcmp(built.data(), kGolden, kGoldenLen) != 0) return false;

    // And parse it back.
    FpvFrame f;
    if (!FpvFrame::parse(kGolden, kGoldenLen, f)) return false;
    if (f.src != FpvFrame::DEV_PARAM_AD_TOOL)     return false;
    if (f.dst != FpvFrame::DEV_TRANSMISSION)      return false;
    if (f.cmdId != FpvFrame::CMD_ID_UPGRADE)      return false;
    if (f.subCmd != SUB_START)                    return false;
    if (f.seq != 0)                               return false;
    if (f.data.size() != 40)                      return false;

    // Corrupting any byte must be rejected (proves the CRCs are actually checked).
    std::vector<uint8_t> bad(kGolden, kGolden + kGoldenLen);
    bad[20] ^= 0xFF;
    FpvFrame ignored;
    if (FpvFrame::parse(bad.data(), bad.size(), ignored)) return false;

    // Two more frames from the successful-upgrade capture, exercising an empty
    // payload and a non-zero SEQ.
    static const uint8_t kConfirm[] = {   // SUBCMD 5, seq 1
        0xAA,0x09,0x03,0x00,0x00,0xF9,0x01,0x00,0xD0,0x1E,0x6F,0x05,0x6F,0xE8 };
    static const uint8_t kVersion[] = {   // SUBCMD 6, seq 2
        0xAA,0x09,0x03,0x00,0x00,0xF9,0x02,0x00,0xD0,0x1E,0x6F,0x06,0xEC,0x16 };

    std::vector<uint8_t> c = FpvFrame::build(
        FpvFrame::DEV_PARAM_AD_TOOL, FpvFrame::DEV_TRANSMISSION,
        FpvFrame::CMD_ID_UPGRADE, SUB_CONFIRM, 1, {}, true);
    if (c.size() != sizeof(kConfirm) || std::memcmp(c.data(), kConfirm, c.size()) != 0)
        return false;

    std::vector<uint8_t> v = FpvFrame::build(
        FpvFrame::DEV_PARAM_AD_TOOL, FpvFrame::DEV_TRANSMISSION,
        FpvFrame::CMD_ID_UPGRADE, SUB_VERSION, 2, {}, true);
    if (v.size() != sizeof(kVersion) || std::memcmp(v.data(), kVersion, v.size()) != 0)
        return false;

    return true;
}

} // namespace unircsdk
