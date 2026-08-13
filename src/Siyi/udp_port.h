#pragma once
//
// POSIX UDP socket for the SIYI FPV (image-transmission) upgrade link.
//
// Mirrors serial_port.h in style and constraints: std + POSIX only, no Qt, no
// third-party libs, so it builds on Android (the UniRC 7 Pro's OS) where
// QUdpSocket pulls in QtNetwork.
//
// Deliberately UNCONNECTED (sendto/recvfrom rather than connect()), matching
// UniGCS's java.net.DatagramSocket usage exactly: it sends to an explicit
// address and accepts a reply from any source. Replies therefore carry their
// sender address out of recv() so the caller can decide whether to trust it —
// a connect()ed socket would silently drop a reply that came back from a
// different source port, which we have not yet observed either way.
//
#include <cstdint>
#include <cstddef>
#include <string>

namespace unircsdk {

class UdpPort {
public:
    UdpPort() : fd_(-1), peerPort_(0) {}
    ~UdpPort() { close(); }

    // Non-copyable (owns a file descriptor).
    UdpPort(const UdpPort&) = delete;
    UdpPort& operator=(const UdpPort&) = delete;

    // Create the socket and record the peer (e.g. "192.168.144.12", 37250).
    // Does not bind a fixed local port (ephemeral, like the Java). Any
    // previously open socket is closed first. Returns false on bad address.
    bool open(const std::string& ip, uint16_t port);

    // Send one datagram. Returns true if the whole payload went out.
    bool send(const uint8_t* buf, size_t len);

    // Wait up to timeoutMs for one datagram. Returns bytes received, 0 on
    // timeout, -1 on error. If non-null, `fromIp` receives the sender address.
    int recv(uint8_t* buf, size_t len, int timeoutMs, std::string* fromIp = nullptr);

    void close();

    bool isOpen() const { return fd_ >= 0; }

    const std::string& peerIp() const { return peerIp_; }
    uint16_t peerPort() const { return peerPort_; }

private:
    int fd_;
    std::string peerIp_;
    uint16_t peerPort_;
    // Stored as raw bytes to keep <netinet/in.h> out of this header.
    uint32_t peerAddrBe_ = 0;  // network byte order
};

} // namespace unircsdk
