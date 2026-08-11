#include "rcu_session.h"

#include "crc.h"

#include <algorithm>
#include <chrono>

namespace unircsdk {

long long RcuSession::nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void RcuSession::sleepMs(long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void RcuSession::setChannelListener(ChannelListener l) {
    std::lock_guard<std::mutex> lk(listenerMutex_);
    channelListener_ = std::move(l);
}

void RcuSession::setFrameListener(FrameListener l) {
    std::lock_guard<std::mutex> lk(listenerMutex_);
    frameListener_ = std::move(l);
}

std::array<int16_t, 16> RcuSession::channels() {
    return channelsCopy();
}

std::array<int16_t, 16> RcuSession::channelsCopy() {
    std::lock_guard<std::mutex> lk(channelsMutex_);
    return channels_;
}

bool RcuSession::isReceiving(long withinMs) {
    long long last = lastChannelMs_.load();
    return last != 0 && (nowMs() - last) <= withinMs;
}

// ---------------------------------------------------------------- lifecycle
bool RcuSession::open() {
    std::lock_guard<std::mutex> lk(lifecycleMutex_);
    if (running_.load()) return true;

    // The INTERNAL RC-MCU link runs at 230400 (NOT 115200 like the external SDK).
    if (!port_.open(port_name_, BAUD)) return false;

    running_.store(true);

    // open the link
    send(TARGET_RC_MCU, CMD_OPEN, std::vector<uint8_t>());

    rxThread_ = std::thread(&RcuSession::rxLoop, this);
    kaThread_ = std::thread(&RcuSession::keepAliveLoop, this);
    return true;
}

void RcuSession::close() {
    {
        std::lock_guard<std::mutex> lk(lifecycleMutex_);
        if (!running_.load()) return;
        running_.store(false);
    }
    // Threads stop calling into the port (rx read has a VTIME timeout, keepalive
    // checks running_), then we can safely close the fd.
    if (rxThread_.joinable()) rxThread_.join();
    if (kaThread_.joinable()) kaThread_.join();
    port_.close();
}

void RcuSession::keepAliveLoop() {
    while (running_.load()) {
        send(TARGET_F0, CMD_KEEPALIVE, std::vector<uint8_t>());
        sleepMs(keepAliveMs_);
    }
}

void RcuSession::rxLoop() {
    uint8_t rd[1024];
    while (running_.load()) {
        int n = port_.read(rd, sizeof(rd));
        if (n < 0) break;
        if (n > 0) feed(rd, static_cast<size_t>(n));
        // n == 0: read timeout, loop and re-check running_
    }
}

// ---------------------------------------------------------------- send
bool RcuSession::send(int target, int cmd, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lk(writeMutex_);
    if (!port_.isOpen()) return false;
    std::vector<uint8_t> f = buildFrame(target, cmd, data);
    return port_.write(f.data(), f.size());
}

std::vector<uint8_t> RcuSession::buildFrame(int target, int cmd,
                                            const std::vector<uint8_t>& data) {
    size_t dl = data.size();
    int s = seq_++ & 0xFFFF;
    std::vector<uint8_t> f(12 + dl + 2);
    f[0] = 0xAA; f[1] = 0x09; f[2] = 0x03;
    f[3] = static_cast<uint8_t>(dl & 0xFF);
    f[4] = static_cast<uint8_t>((dl >> 8) & 0xFF);
    f[5] = crc8_maxim(f.data(), 5);                 // CRC-8/MAXIM over the 5-byte header
    f[6] = static_cast<uint8_t>(s & 0xFF);
    f[7] = static_cast<uint8_t>((s >> 8) & 0xFF);
    f[8] = 0xD0; f[9] = 0x10; f[10] = static_cast<uint8_t>(target);
    f[11] = static_cast<uint8_t>(cmd);
    for (size_t i = 0; i < dl; ++i) f[12 + i] = data[i];
    uint16_t crc = crc16_xmodem(f.data(), f.size() - 2);
    f[f.size() - 2] = static_cast<uint8_t>(crc & 0xFF);
    f[f.size() - 1] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    return f;
}

// ---------------------------------------------------------------- config
void RcuSession::setFailsafeEnabled(bool en) {
    send(TARGET_RC_MCU, CMD_FAILSAFE, std::vector<uint8_t>{ static_cast<uint8_t>(en ? 1 : 0) });
}

void RcuSession::setDeadzone(int v) {
    v = std::max(10, std::min(80, v));
    send(TARGET_RC_MCU, CMD_DEADZONE, std::vector<uint8_t>{ static_cast<uint8_t>(v) });
}

void RcuSession::setFlightMode(int m) {
    send(TARGET_RC_MCU, CMD_FLIGHT_MODE, std::vector<uint8_t>{ static_cast<uint8_t>(m) });
}

void RcuSession::setFlightChannel(int ch) {
    ch = std::max(1, std::min(16, ch));            // 1-based comm channel
    send(TARGET_RC_MCU, CMD_FLIGHT_CHANNEL, std::vector<uint8_t>{ static_cast<uint8_t>(ch) });
}

void RcuSession::requestGet(int getCmd, int target) {
    send(target, getCmd, std::vector<uint8_t>());  // empty payload = query; reply on same cmd id
}

void RcuSession::setSdkConnectType(int t) {
    send(TARGET_RC_MCU, CMD_SET_SDK_CONNECT, std::vector<uint8_t>{ static_cast<uint8_t>(t) });
}

void RcuSession::setChannel15Mode(int m) {
    send(TARGET_RC_MCU, CMD_CHANNEL15,
         std::vector<uint8_t>{ 0, 0, 0, 1, static_cast<uint8_t>(m) });
}

void RcuSession::setButtonMode(int buttonId, int mode) {
    send(TARGET_RC_MCU, CMD_BUTTON_MODE,
         std::vector<uint8_t>{ static_cast<uint8_t>(buttonId), static_cast<uint8_t>(mode) });
}

void RcuSession::setChannelAdaption(bool en, int ch) {
    send(TARGET_IMAGE, CMD_CHANNEL_ADAPTION,
         std::vector<uint8_t>{ static_cast<uint8_t>(en ? 1 : 0), static_cast<uint8_t>(ch) });
}

// ---------------------------------------------------------------- calibration
void RcuSession::calStart(int calTarget) {
    send(TARGET_RC_MCU, CMD_CALIBRATION,
         std::vector<uint8_t>{ static_cast<uint8_t>(calTarget), static_cast<uint8_t>(CAL_START) });
}
void RcuSession::calPoll(int calTarget) {
    send(TARGET_RC_MCU, CMD_CALIBRATION,
         std::vector<uint8_t>{ static_cast<uint8_t>(calTarget), static_cast<uint8_t>(CAL_POLL) });
}
void RcuSession::calSave(int calTarget) {
    send(TARGET_RC_MCU, CMD_CALIBRATION,
         std::vector<uint8_t>{ static_cast<uint8_t>(calTarget), static_cast<uint8_t>(CAL_SAVE) });
}

int RcuSession::runCalibration(int calTarget, CalProgress cb, long sweepMillis) {
    calStep_.store(0);
    int last = -1;
    long long maxMinStart = 0;
    bool sawMedian = false, sawMaxMin = false;
    (void)sawMedian; // observed for parity with the Java flow; gate uses sawMaxMin

    calStart(calTarget);
    long long t0 = nowMs();
    while (nowMs() - t0 < 240000) {
        calPoll(calTarget);
        int s = calStep_.load();
        if (s == 1) sawMedian = true;
        if (s == 2) sawMaxMin = true;
        // SUCCESS(3)/FAILED(4) only count once we've progressed through the
        // sequence — the START-ack is ALSO 0x03, so ignore it until MAX_MIN.
        if (s != last) {
            last = s;
            if (cb && (s == 1 || s == 2 || ((s == 3 || s == 4) && sawMaxMin))) cb(s);
            if ((s == 3 || s == 4) && sawMaxMin) return s;
        }
        if (s == 2) {
            if (maxMinStart == 0) maxMinStart = nowMs();
            if (nowMs() - maxMinStart >= sweepMillis) {
                calSave(calTarget);
                long long t1 = nowMs();
                while (nowMs() - t1 < 5000) {
                    calPoll(calTarget);
                    int cs = calStep_.load();
                    if (cs == 3 || cs == 4) { if (cb) cb(cs); return cs; }
                    sleepMs(150);
                }
                return calStep_.load();
            }
        }
        sleepMs(150);
    }
    return calStep_.load();
}

// ---------------------------------------------------------------- receive
void RcuSession::feed(const uint8_t* data, size_t n) {
    acc_.insert(acc_.end(), data, data + n);
    if (acc_.size() > 8192) {
        acc_.erase(acc_.begin(), acc_.end() - 4096);
    }
    const uint8_t* a = acc_.data();
    size_t len = acc_.size();
    size_t i = 0;
    while (i + 14 <= len) {
        // frame header: AA | 09|0A | 03
        if (a[i] != 0xAA || a[i + 2] != 0x03 ||
            (a[i + 1] != 0x0A && a[i + 1] != 0x09)) { ++i; continue; }
        int dl = a[i + 3] | (a[i + 4] << 8);
        if (dl > 512) { ++i; continue; }
        size_t total = 12 + static_cast<size_t>(dl) + 2;
        if (i + total > len) break;
        uint16_t crcCalc = crc16_xmodem(a + i, total - 2);
        uint16_t crcPkt = static_cast<uint16_t>(
            a[i + total - 2] | (a[i + total - 1] << 8));
        if (crcCalc == crcPkt) {
            int cmd = a[i + 11];
            std::vector<uint8_t> d(a + i + 12, a + i + 12 + dl);
            dispatch(cmd, d);
            i += total;
        } else {
            ++i;
        }
    }
    if (i > 0) acc_.erase(acc_.begin(), acc_.begin() + i);
}

void RcuSession::dispatch(int cmd, const std::vector<uint8_t>& d) {
    if (cmd == CMD_CALIBRATION && d.size() >= 2) {
        calStep_.store(d[1]);                       // reply data = [target, step, status]
    }
    if (cmd == CMD_CHANNELS && d.size() >= 8) {
        size_t cnt = std::min(d.size() / 2, static_cast<size_t>(16));
        {
            std::lock_guard<std::mutex> lk(channelsMutex_);
            for (size_t c = 0; c < cnt; ++c)
                channels_[c] = static_cast<int16_t>(d[c * 2] | (d[c * 2 + 1] << 8));
        }
        lastChannelMs_.store(nowMs());
        ChannelListener l;
        { std::lock_guard<std::mutex> lk(listenerMutex_); l = channelListener_; }
        if (l) l(channelsCopy());
    }
    FrameListener fl;
    { std::lock_guard<std::mutex> lk(listenerMutex_); fl = frameListener_; }
    if (fl) fl(cmd, d);
}

} // namespace unircsdk
