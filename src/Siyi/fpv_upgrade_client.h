#pragma once
//
// FpvUpgradeClient — firmware upgrade for the SIYI ground unit's FPV
// (image-transmission / video+telemetry datalink) module, over UDP :37250.
//
//   FpvUpgradeClient c;
//   if (!c.open()) { /* 192.168.144.12:37250 */ }
//   std::string ver;
//   if (c.queryVersion(ver)) { /* link is alive */ }
//
// SCOPE — this is NOT the RC handheld's own MCU and NOT the gimbal/camera:
//
//   target                     transport                    driver
//   ------------------------   --------------------------   ------------------
//   RC handheld MCU            ttyHS1 serial, cmd 32/35/38  RcuSession (todo)
//   aircraft RC receiver       ttyHS1 serial, cmd 32/35/38  RcuSession (todo)
//   FPV datalink (ground/air)  UDP :37250, CMD_ID 111       THIS CLASS
//   gimbal / camera            UDP :37280                   (not implemented)
//
// UDP is not merely the observed path, it is the only one: the serial upgrade
// commands hardcode their destination to RCU(16)/Receiver(19) and can never
// address Transmission(30). See FIRMWARE_UPGRADE_PROTOCOL.md §2.
//
// PREREQUISITE: the internal 192.168.144.x network between the Android SoC and
// the radio/video module must be up. If it is not, every send here goes into a
// black hole and each step times out — that is an environment fault, not a
// protocol one. queryVersion() is the cheap liveness probe for it.
//
// ############################  HOW THE TRANSFER ACTUALLY WORKS  #############
//
// The firmware bytes do NOT travel over this UDP channel. A full successful
// ground-unit upgrade was captured end to end; the UDP channel is CONTROL only,
// and the payload goes over FTP:
//
//   1. UDP  SUBCMD 2  -> firmware filename        <- [01, u16]  (ok + see below)
//   2. FTP  STOR the firmware file to the unit    (anonymous, binary, PASV)
//   3. UDP  SUBCMD 5  -> (no payload)             <- [01]       module flashes+reboots
//   4. UDP  SUBCMD 6  polled ~2s until it answers with the new version string
//
// An earlier revision of this file assumed a UDP-chunked transfer (SUBCMD 3/4)
// because that is what the decompiled code suggested. The capture disproved it:
// zero SUBCMD 3/4 frames were sent. Those subcommands belong to the OTHER
// file-transfer mode (h4.a.UDP) which this hardware does not use — the ground
// unit is configured h4.a.FTP. sendChunk()/sendFinish() are kept only to
// document that mode and are not part of this path.
//
// Reboot is slow: ~97s from CONFIRM to the first successful version reply in
// the capture (UniGCS waited ~31s before even starting to poll).
// ############################################################################
//
// std + POSIX only; no Qt. Blocking calls — run off the GUI thread.
//
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>

#include "udp_port.h"
#include "fpv_frame.h"

namespace unircsdk {

class FpvUpgradeClient {
public:
    // ---- endpoint ----
    // Ground unit default. The real address may differ and is discoverable via
    // the RCU-serial GET_IMAGE_TRANS_IP query (group 20 / cmd 117) — not yet
    // implemented in RcuSession; pass it explicitly to open() if you have it.
    static constexpr const char* DEFAULT_GROUND_IP = "192.168.144.12";
    static constexpr const char* DEFAULT_AIR_IP    = "192.168.144.11";
    static constexpr uint16_t    PORT              = 37250;

    // ---- SUBCMD ids (CMD_ID is always 111) ----
    static constexpr uint8_t SUB_START   = 2;  // -> filename;      <- [ok:u8, u16 LE] (see startAck)
    static constexpr uint8_t SUB_CHUNK   = 3;  // UDP-transfer mode only — NOT used by this hardware
    static constexpr uint8_t SUB_FINISH  = 4;  // UDP-transfer mode only — NOT used by this hardware
    static constexpr uint8_t SUB_CONFIRM = 5;  // -> (none);        <- [status:i8]  applies + reboots
    static constexpr uint8_t SUB_VERSION = 6;  // -> (none);        <- ASCII version string

    // ---- FTP endpoint (bulk transfer) ----
    // Both 200 and 209 were found running the same anonymous FTP server on the
    // ground unit. The upgrade path's own config constant is 209; a debug screen
    // in the app uses 200. 209 is used here to match the upgrade path.
    static constexpr uint16_t FTP_PORT = 209;
    static constexpr uint16_t FTP_PORT_ALT = 200;

    // Per-socket-operation FTP timeouts. The ground unit is on a wired-speed
    // internal link; the air unit is across the RF link and measured ~85x
    // slower (10.2 MB/s vs 0.12 MB/s), which puts a 111 MB firmware at roughly
    // 15 MINUTES. The default is far too tight for that, hence the second value.
    static constexpr int FTP_TIMEOUT_MS      = 30000;
    static constexpr int FTP_TIMEOUT_SLOW_MS = 120000;   // use for the air unit

    // Result codes. Values mirror biz.siyi.m5.d's raw wire ints; note the
    // decompiled mapper treats ANY unlisted value as OK, which fromWire()
    // reproduces faithfully rather than "fixing".
    enum class Result {
        Ok,
        PackRepeat,                // 3  — resend this packet
        ErrPackNum,                // -1
        ErrFile,                   // -3
        ErrCheckSum,               // -7
        ErrWriteFileSize,          // -10
        ErrFileSize,               // -13
        ErrConfigVersionMismatch,  // -15
        Timeout,                   // no reply within the deadline
        BadReply,                  // malformed / mismatched frame
        NotImplemented             // step not yet verified on the wire
    };
    static const char* resultName(Result r);
    static Result fromWire(int8_t code);

    // Retransmit policy. Defaults are what UniGCS was observed doing on this
    // hardware: SUBCMD 2 resent 10x at ~2.00s, identical payload, then give up.
    // (The decompile also carries a 300ms ground / 500ms air field whose role is
    // unconfirmed and which does NOT match the observed 2s spacing — hence the
    // observed values are used here.)
    static constexpr int DEFAULT_RETRIES      = 10;
    static constexpr int DEFAULT_RETRY_MS     = 2000;
    static constexpr int DEFAULT_REPLY_WAIT_MS = 2000;

    FpvUpgradeClient() = default;
    ~FpvUpgradeClient() { close(); }

    FpvUpgradeClient(const FpvUpgradeClient&) = delete;
    FpvUpgradeClient& operator=(const FpvUpgradeClient&) = delete;

    bool open(const std::string& ip = DEFAULT_GROUND_IP, uint16_t port = PORT);
    void close();
    bool isOpen() const { return port_.isOpen(); }

    // Endpoint the client is talking to (same host the FTP transfer targets).
    const std::string& peerIp() const { return port_.peerIp(); }

    // ---- implemented ----

    // SUBCMD 6. Read-only liveness/identity probe; also the cheapest way to tell
    // whether 192.168.144.x is actually reachable. Reply is an ASCII string.
    bool queryVersion(std::string& out, int timeoutMs = DEFAULT_REPLY_WAIT_MS);

    // SUBCMD 2. Announces the firmware filename before the FTP upload. Both the
    // send bytes and the reply are confirmed from capture. UniGCS gates on the
    // name's prefix (substring before the first '_') matching the unit's
    // hardware id, so pass the real name.
    //
    // `startAck` receives the u16 the unit returns alongside its ok flag. Its
    // MEANING IS UNKNOWN: the one capture returned 32000 (0x7D00). It is not
    // needed for the FTP path, so it is surfaced rather than interpreted — do
    // not assume it is a file id, size or packet count.
    //
    // (Historical note: the captured 40-byte field looked like a 39-char name
    // plus a stray 0x31. It is not stray — SIYI's firmware files genuinely end
    // in ".zip1". Passing the real basename is therefore correct, and no special
    // handling is needed.)
    Result sendStart(const std::string& filename, uint16_t& startAck,
                     int retries = DEFAULT_RETRIES,
                     int retryMs = DEFAULT_RETRY_MS);

    // SUBCMD 5. Confirmed from capture. Tells the unit to flash what was
    // uploaded and reboot. Returns as soon as the unit acknowledges; the reboot
    // itself takes ~1.5 minutes (use waitForReboot()).
    Result sendConfirm(int timeoutMs = DEFAULT_REPLY_WAIT_MS);

    /// How long the unit must answer CONTINUOUSLY before a reboot counts as
    /// finished. It reboots twice, and it also keeps answering briefly after
    /// CONFIRM, so anything shorter can report success mid-flash.
    static constexpr int REBOOT_STABLE_MS = 90000;

    // Wait out the post-CONFIRM reboot: first for the unit to drop off, then for
    // it to come back and stay back for REBOOT_STABLE_MS. Returns true and sets
    // `versionOut` only once it is genuinely settled.
    //
    // This deliberately does NOT return on the first successful reply — right
    // after CONFIRM that reply comes from the OLD firmware, still running.
    // Budget several minutes: observed ~97s to the first reply, then a further
    // reboot of ~65s.
    bool waitForReboot(std::string& versionOut, int overallTimeoutMs = 420000,
                       int pollMs = 2000);

    // Which step `upgrade()` is on, for UI reporting.
    enum class Phase {
        Announce,   // SUBCMD 2
        Upload,     // FTP STOR   (percent is meaningful here)
        Verify,     // FTP SIZE   (integrity gate before anything is flashed)
        Commit,     // SUBCMD 5   — past this point the module is being written
        Reboot      // polling SUBCMD 6
    };
    static const char* phaseName(Phase p);

    // Full ground-unit upgrade: START -> FTP STOR -> size verify -> CONFIRM ->
    // wait for reboot. `progress` reports the current phase, and 0-100 during
    // Upload (-1 when a percentage is not meaningful).
    //
    // The size verify is not optional: if the server does not report exactly the
    // local byte count, this returns ErrFileSize and does NOT commit. Flashing a
    // truncated upload is the realistic way to brick the module.
    using ProgressFn = std::function<void(Phase phase, int percent)>;
    Result upgrade(const std::string& binPath,
                   ProgressFn progress = nullptr,
                   std::string* newVersionOut = nullptr,
                   uint16_t ftpPort = FTP_PORT,
                   int ftpTimeoutMs = FTP_TIMEOUT_MS);

    // ---- other transfer mode (h4.a.UDP); NOT used by this hardware ----
    // Retained as documentation only. Never observed on the wire; byte layouts
    // are decompile-only. Both return NotImplemented.
    Result sendChunk(uint16_t fileId, const uint8_t* data, size_t len, uint16_t& packNumOut);
    Result sendFinish(uint32_t fileLen, const std::string& md5Hex);

    // Build/parse round-trip check against the real captured START frame.
    // Returns true if this code reproduces UniGCS's bytes exactly. Cheap; call
    // it from a unit test or at startup in debug builds.
    static bool selfTest();

private:
    // Send one frame and wait for a reply carrying the same SUBCMD. Replies
    // arrive with SRC/DST swapped relative to what we sent.
    bool transact(uint8_t subCmd, const std::vector<uint8_t>& payload,
                  FpvFrame& reply, int timeoutMs);

    UdpPort  port_;
    uint16_t seq_ = 0;
};

} // namespace unircsdk
