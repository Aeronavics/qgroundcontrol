#pragma once
//
// POSIX serial port (open() + termios), raw 8N1. Replaces the Java
// FileInputStream/FileOutputStream + `stty` transport with a real termios
// configuration so it works on Android/Linux where QSerialPort is unavailable.
//
// std + POSIX only. No Qt, no third-party libs.
//
#include <cstdint>
#include <cstddef>
#include <string>

namespace unircsdk {

class SerialPort {
public:
    SerialPort() : fd_(-1) {}
    ~SerialPort() { close(); }

    // Non-copyable (owns a file descriptor).
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // Open `dev` (e.g. "/dev/ttyHS1") at `baud` (e.g. 230400 or 115200) as raw
    // 8N1, no flow control. Returns true on success. Any previously open port is
    // closed first.
    bool open(const std::string& dev, int baud);

    // Blocking-with-timeout read (VMIN=0, VTIME=1 => ~100ms). Returns the number
    // of bytes read (0 on timeout, may be 0 repeatedly) or -1 on error/EOF.
    int read(uint8_t* buf, size_t len);

    // Write all `len` bytes. Returns true if the whole buffer was written.
    bool write(const uint8_t* buf, size_t len);

    void close();

    bool isOpen() const { return fd_ >= 0; }

private:
    int fd_;
};

} // namespace unircsdk
