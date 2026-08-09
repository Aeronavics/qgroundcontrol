#include "serial_port.h"

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

namespace unircsdk {

namespace {
// Map an integer baud to a termios speed_t constant. Returns 0 if unsupported.
speed_t toSpeed(int baud) {
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;   // External SDK, /dev/ttyHS3
        case 230400: return B230400;   // Internal RCU link, /dev/ttyHS1
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
        default:     return 0;
    }
}
} // namespace

bool SerialPort::open(const std::string& dev, int baud) {
    close();

    speed_t sp = toSpeed(baud);
    if (sp == 0) return false;

    int fd = ::open(dev.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return false;

    // Switch back to blocking mode; VMIN/VTIME below govern read timing.
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);

    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) { ::close(fd); return false; }

    cfmakeraw(&tio);                       // raw mode: no echo/canon/signals
    cfsetispeed(&tio, sp);
    cfsetospeed(&tio, sp);

    tio.c_cflag |= (CLOCAL | CREAD);       // ignore modem lines, enable receiver
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;                    // 8 data bits
    tio.c_cflag &= ~PARENB;                // no parity
    tio.c_cflag &= ~CSTOPB;                // 1 stop bit
#ifdef CRTSCTS
    tio.c_cflag &= ~CRTSCTS;               // no hardware flow control
#endif
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);// no software flow control

    tio.c_cc[VMIN]  = 0;                   // non-blocking-ish: return on timeout
    tio.c_cc[VTIME] = 1;                   // 0.1s inter-byte read timeout

    if (tcsetattr(fd, TCSANOW, &tio) != 0) { ::close(fd); return false; }
    tcflush(fd, TCIOFLUSH);

    fd_ = fd;
    return true;
}

int SerialPort::read(uint8_t* buf, size_t len) {
    if (fd_ < 0) return -1;
    ssize_t n = ::read(fd_, buf, len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EINTR) return 0; // treat as "no data yet"
        return -1;
    }
    return static_cast<int>(n);
}

bool SerialPort::write(const uint8_t* buf, size_t len) {
    if (fd_ < 0) return false;
    size_t total = 0;
    while (total < len) {
        ssize_t n = ::write(fd_, buf + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

void SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace unircsdk
