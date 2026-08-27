#include "udp_port.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

namespace unircsdk {

bool UdpPort::open(const std::string& ip, uint16_t port) {
    close();

    struct in_addr addr;
    if (inet_pton(AF_INET, ip.c_str(), &addr) != 1) return false;

    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;

    fd_         = fd;
    peerIp_     = ip;
    peerPort_   = port;
    peerAddrBe_ = addr.s_addr;
    return true;
}

bool UdpPort::send(const uint8_t* buf, size_t len) {
    if (fd_ < 0) return false;

    struct sockaddr_in dst;
    std::memset(&dst, 0, sizeof(dst));
    dst.sin_family      = AF_INET;
    dst.sin_port        = htons(peerPort_);
    dst.sin_addr.s_addr = peerAddrBe_;

    for (;;) {
        ssize_t n = ::sendto(fd_, buf, len, 0,
                             reinterpret_cast<struct sockaddr*>(&dst), sizeof(dst));
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        return static_cast<size_t>(n) == len;
    }
}

int UdpPort::recv(uint8_t* buf, size_t len, int timeoutMs, std::string* fromIp) {
    if (fd_ < 0) return -1;

    struct timeval tv;
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) return -1;

    struct sockaddr_in src;
    socklen_t srcLen = sizeof(src);
    std::memset(&src, 0, sizeof(src));

    for (;;) {
        ssize_t n = ::recvfrom(fd_, buf, len, 0,
                               reinterpret_cast<struct sockaddr*>(&src), &srcLen);
        if (n < 0) {
            if (errno == EINTR) continue;
            // SO_RCVTIMEO expiry surfaces as EAGAIN/EWOULDBLOCK => timeout, not error.
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            return -1;
        }
        if (fromIp) {
            char s[INET_ADDRSTRLEN] = {0};
            if (inet_ntop(AF_INET, &src.sin_addr, s, sizeof(s))) *fromIp = s;
            else fromIp->clear();
        }
        return static_cast<int>(n);
    }
}

void UdpPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace unircsdk
