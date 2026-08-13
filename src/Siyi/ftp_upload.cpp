#include "ftp_upload.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <vector>

namespace unircsdk {

namespace {

constexpr size_t kChunk = 32 * 1024;

class Sock {
public:
    Sock() : fd_(-1) {}
    ~Sock() { close(); }
    Sock(const Sock&) = delete;
    Sock& operator=(const Sock&) = delete;

    bool connect(const std::string& ip, uint16_t port, int timeoutMs) {
        close();
        struct in_addr a;
        if (inet_pton(AF_INET, ip.c_str(), &a) != 1) return false;

        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return false;

        struct timeval tv;
        tv.tv_sec  = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        struct sockaddr_in sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port   = htons(port);
        sa.sin_addr   = a;

        if (::connect(fd, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) != 0) {
            ::close(fd);
            return false;
        }
        fd_ = fd;
        return true;
    }

    bool writeAll(const void* buf, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(buf);
        size_t done = 0;
        while (done < len) {
            ssize_t n = ::write(fd_, p + done, len - done);
            if (n < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (n == 0) return false;
            done += static_cast<size_t>(n);
        }
        return true;
    }

    // Read one complete FTP reply (handles "NNN-" multi-line continuations).
    // Returns the numeric code, or -1 on error/timeout. Appends raw text to out.
    int readReply(std::string* out = nullptr) {
        std::string acc;
        char buf[512];
        for (;;) {
            ssize_t n = ::read(fd_, buf, sizeof(buf));
            if (n < 0) {
                if (errno == EINTR) continue;
                return -1;
            }
            if (n == 0) return -1;
            acc.append(buf, static_cast<size_t>(n));

            // A reply is complete when a line starts with 3 digits + ' '.
            size_t lineStart = 0;
            for (size_t i = 0; i + 1 < acc.size(); ++i) {
                if (acc[i] != '\n') continue;
                // examine the line that starts at lineStart
                if (i - lineStart >= 4) {
                    const char* L = acc.c_str() + lineStart;
                    if (L[0] >= '0' && L[0] <= '9' &&
                        L[1] >= '0' && L[1] <= '9' &&
                        L[2] >= '0' && L[2] <= '9' && L[3] == ' ') {
                        if (out) *out = acc;
                        return (L[0]-'0')*100 + (L[1]-'0')*10 + (L[2]-'0');
                    }
                }
                lineStart = i + 1;
            }
            // Also handle a final line without trailing newline yet.
            if (acc.size() - lineStart >= 4) {
                const char* L = acc.c_str() + lineStart;
                if (L[0] >= '0' && L[0] <= '9' &&
                    L[1] >= '0' && L[1] <= '9' &&
                    L[2] >= '0' && L[2] <= '9' && L[3] == ' ' &&
                    acc.find('\n', lineStart) != std::string::npos) {
                    if (out) *out = acc;
                    return (L[0]-'0')*100 + (L[1]-'0')*10 + (L[2]-'0');
                }
            }
            if (acc.size() > 64 * 1024) return -1;   // runaway guard
        }
    }

    int cmd(const std::string& line, std::string* out = nullptr) {
        std::string s = line;
        s += "\r\n";
        if (!writeAll(s.data(), s.size())) return -1;
        return readReply(out);
    }

    void close() {
        if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
    }
    int fd() const { return fd_; }

private:
    int fd_;
};

// Parse "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)".
bool parsePasv(const std::string& reply, std::string& ip, uint16_t& port) {
    size_t open = reply.find('(');
    size_t close = reply.find(')', open == std::string::npos ? 0 : open);
    if (open == std::string::npos || close == std::string::npos) return false;
    int v[6] = {0,0,0,0,0,0};
    if (std::sscanf(reply.c_str() + open + 1, "%d,%d,%d,%d,%d,%d",
                    &v[0],&v[1],&v[2],&v[3],&v[4],&v[5]) != 6) return false;
    for (int i = 0; i < 6; ++i) if (v[i] < 0 || v[i] > 255) return false;

    char b[32];
    std::snprintf(b, sizeof(b), "%d.%d.%d.%d", v[0], v[1], v[2], v[3]);
    ip   = b;
    port = static_cast<uint16_t>((v[4] << 8) | v[5]);
    return true;
}

FtpResult fail(int code, const std::string& msg) {
    FtpResult r;
    r.ok = false;
    r.replyCode = code;
    r.error = msg;
    return r;
}

} // namespace

FtpResult ftpStoreFile(const std::string& host, uint16_t port,
                       const std::string& remoteName, const std::string& localPath,
                       std::function<void(uint64_t, uint64_t)> progress,
                       const std::string& user, const std::string& pass,
                       int timeoutMs) {
    FILE* f = std::fopen(localPath.c_str(), "rb");
    if (!f) return fail(0, "cannot open local file: " + localPath);

    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz < 0) { std::fclose(f); return fail(0, "cannot size local file"); }
    const uint64_t total = static_cast<uint64_t>(sz);

    Sock ctrl;
    if (!ctrl.connect(host, port, timeoutMs)) {
        std::fclose(f);
        return fail(0, "control connect failed to " + host);
    }

    int code = ctrl.readReply();                 // greeting
    if (code != 220) { std::fclose(f); return fail(code, "bad FTP greeting"); }

    code = ctrl.cmd("USER " + user);
    // 230 = logged in outright (what this device does); 331 = password wanted.
    if (code == 331) code = ctrl.cmd("PASS " + pass);
    if (code != 230) { std::fclose(f); return fail(code, "login rejected"); }

    code = ctrl.cmd("TYPE I");
    if (code != 200) { std::fclose(f); return fail(code, "TYPE I rejected"); }

    std::string pasvReply;
    code = ctrl.cmd("PASV", &pasvReply);
    if (code != 227) { std::fclose(f); return fail(code, "PASV rejected"); }

    std::string dataIp;
    uint16_t dataPort = 0;
    if (!parsePasv(pasvReply, dataIp, dataPort)) {
        std::fclose(f);
        return fail(code, "unparseable PASV reply: " + pasvReply);
    }
    // Some embedded servers advertise 0.0.0.0 / an internal address in PASV;
    // the control host is always the right place to connect.
    if (dataIp == "0.0.0.0") dataIp = host;

    Sock data;
    if (!data.connect(dataIp, dataPort, timeoutMs)) {
        std::fclose(f);
        return fail(0, "data connect failed");
    }

    code = ctrl.cmd("STOR " + remoteName);
    if (code != 150 && code != 125) {
        std::fclose(f);
        return fail(code, "STOR rejected");
    }

    std::vector<uint8_t> buf(kChunk);
    uint64_t sent = 0;
    for (;;) {
        size_t n = std::fread(buf.data(), 1, buf.size(), f);
        if (n == 0) break;
        if (!data.writeAll(buf.data(), n)) {
            std::fclose(f);
            return fail(0, "data write failed");
        }
        sent += n;
        if (progress) progress(sent, total);
    }
    std::fclose(f);

    data.close();                    // EOF signals end-of-transfer
    code = ctrl.readReply();         // expect 226 Transfer complete
    if (code != 226 && code != 250) return fail(code, "transfer not acknowledged");

    ctrl.cmd("QUIT");

    FtpResult r;
    r.ok = true;
    r.replyCode = code;
    return r;
}

FtpResult ftpFileSize(const std::string& host, uint16_t port,
                      const std::string& remoteName, uint64_t& sizeOut,
                      const std::string& user, const std::string& pass,
                      int timeoutMs) {
    Sock ctrl;
    if (!ctrl.connect(host, port, timeoutMs))
        return fail(0, "control connect failed to " + host);

    int code = ctrl.readReply();
    if (code != 220) return fail(code, "bad FTP greeting");

    code = ctrl.cmd("USER " + user);
    if (code == 331) code = ctrl.cmd("PASS " + pass);
    if (code != 230) return fail(code, "login rejected");

    // SIZE is only meaningful in binary mode on many servers.
    ctrl.cmd("TYPE I");

    std::string reply;
    code = ctrl.cmd("SIZE " + remoteName, &reply);
    if (code != 213) {
        ctrl.cmd("QUIT");
        return fail(code, "SIZE failed: " + reply);
    }

    // "213 <n>"
    unsigned long long n = 0;
    const size_t sp = reply.find(' ');
    if (sp == std::string::npos || std::sscanf(reply.c_str() + sp + 1, "%llu", &n) != 1) {
        ctrl.cmd("QUIT");
        return fail(code, "unparseable SIZE reply: " + reply);
    }
    sizeOut = static_cast<uint64_t>(n);

    ctrl.cmd("QUIT");

    FtpResult r;
    r.ok = true;
    r.replyCode = code;
    return r;
}

} // namespace unircsdk
