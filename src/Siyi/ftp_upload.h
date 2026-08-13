#pragma once
//
// Minimal FTP upload (STOR) client — just enough for the SIYI FPV firmware
// upgrade, which is where the actual firmware bytes travel.
//
// Deliberately not a general FTP library: anonymous login, binary mode, passive
// mode, one STOR. That is exactly what the ground unit's server needs and keeps
// this dependency-free (std + POSIX only, no Qt, no libcurl) in line with the
// rest of src/Siyi.
//
// Observed server (ground unit 192.168.144.12:209, probed live):
//   220 Operation successful
//   USER/PASS  -> 230 immediately, any credentials (effectively anonymous)
//   SYST       -> 215 UNIX Type: L8
//   PWD        -> 257 "/"
//   FEAT       -> EPSV, PASV, REST STREAM, MDTM, SIZE
// The uploaded file does NOT persist in / after a successful flash — the module
// consumes it — so do not expect to verify by listing afterwards.
//
#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>

namespace unircsdk {

struct FtpResult {
    bool ok = false;
    int  replyCode = 0;      // last FTP reply code seen (0 if none)
    std::string error;       // human-readable failure detail, empty on success
};

// Upload `localPath` to `remoteName` in the server's current directory.
// `progress` (optional) is called with bytes-sent and total.
// Credentials default to the anonymous pair the device accepts.
FtpResult ftpStoreFile(const std::string& host,
                       uint16_t port,
                       const std::string& remoteName,
                       const std::string& localPath,
                       std::function<void(uint64_t sent, uint64_t total)> progress = nullptr,
                       const std::string& user = "anonymous",
                       const std::string& pass = "anonymous",
                       int timeoutMs = 10000);

// Query the size of a file on the server (FTP SIZE). Sets `sizeOut` on success.
//
// Worth using as an integrity gate: after uploading firmware, confirm the
// server reports exactly the local byte count BEFORE telling the module to
// flash it. A truncated upload followed by a commit is how you brick hardware.
FtpResult ftpFileSize(const std::string& host,
                      uint16_t port,
                      const std::string& remoteName,
                      uint64_t& sizeOut,
                      const std::string& user = "anonymous",
                      const std::string& pass = "anonymous",
                      int timeoutMs = 10000);

} // namespace unircsdk
