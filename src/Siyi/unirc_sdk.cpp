#include "unirc_sdk.h"

#include <algorithm>
#include <chrono>

namespace unircsdk {

long long UniRcSdk::nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string UniRcSdk::physName(int type) {
    switch (type) {
        case 0: return "stick/wheel";
        case 1: return "button";
        case 2: return "virtual";
        case 4: return "knob";
        case 5: return "3pos-switch";
        case 6: return "PPM";
        default: return "type" + std::to_string(type);
    }
}

std::string UniRcSdk::Mapping::name() const {
    switch (type) {
        case PHY_STICK_WHEEL:
        {
            switch (entityId) {
                case 0: return "J1";
                case 1: return "J2";
                case 2: return "J3";
                case 3: return "J4";
                case 4: return "LD";
                case 5: return "RD";
                case 8: return "J5";
                case 9: return "J6";
                default: return "";
            }
        }
        case PHY_BUTTON:
        {
            switch (entityId) {
                case 0: return "S1";
                case 1: return "S2";
                case 2: return "S3";
                case 3: return "S4";
                case 4: return "L1";
                case 5: return "L2";
                case 6: return "R1";
                case 7: return "R2";
                case 8: return "R3";
                case 9: return "M1";
                case 10: return "M2";
                case 11: return "M3";
                case 12: return "M4";
                case 13: return "M5";
                case 14: return "M6";
                default: return "";
            }
        }
        case PHY_VIRTUAL:
        {
            switch (entityId) {
                case 0: return " ";
                case 1: return "RSSI";
                default: return "";
            }
        }
        case 3:
        {
            switch (entityId) {
                case 0: return "--";
                default: return "";
            }
        }
        case PHY_KNOB:
        {
            switch (entityId) {
                default: return "";
            }
        }
        case PHY_SWITCH_3POS:
        {
            switch (entityId) {
                case 0: return "SA";
                case 1: return "SB";
                default: return "";
            }
        }
        case PHY_PPM:
        {
            switch (entityId) {
                default: return "";
            }
        }
        default: return "";
    }
}



std::string UniRcSdk::Mapping::toString() const {
    return "CH" + std::to_string(rcChannel) + " <- " + physName(type) +
           " id=" + std::to_string(entityId) + " name: " + name();
}

std::string UniRcSdk::SystemSettings::toString() const {
    return "SystemSettings{bind=" + std::to_string(bindingStatus) +
           ", joyType=" + std::to_string(joyType) +
           ", bat=" + std::to_string(rcBatteryVolts()) + "V}";
}

void UniRcSdk::setChannelListener(ChannelListener l) {
    std::lock_guard<std::mutex> lk(listenerMutex_);
    listener_ = std::move(l);
}

std::array<int16_t, 16> UniRcSdk::channels() {
    std::lock_guard<std::mutex> lk(channelsMutex_);
    return channels_;
}

bool UniRcSdk::isReceiving(long withinMs) {
    long long last = lastChannelMs_.load();
    return last != 0 && (nowMs() - last) <= withinMs;
}

// ---------------------------------------------------------------- lifecycle
bool UniRcSdk::start() {
    if (running_.load()) return true;
    if (!transport_.open(port_name_, BAUD)) return false;
    running_.store(true);
    rxThread_ = std::thread(&UniRcSdk::rxLoop, this);
    startChannelStream(streamFreq_);
    keepAlive_ = std::thread(&UniRcSdk::keepAliveLoop, this);
    return true;
}

void UniRcSdk::close() {
    if (!running_.exchange(false)) return;
    if (rxThread_.joinable()) rxThread_.join();
    if (keepAlive_.joinable()) keepAlive_.join();
    transport_.close();
}

void UniRcSdk::rxLoop() {
    uint8_t buf[1024];
    while (running_.load()) {
        int n = transport_.read(buf, sizeof(buf));
        if (n < 0) break;
        if (n == 0) continue;
        std::vector<UniRcFrame> frames = parser_.feed(buf, static_cast<size_t>(n));
        for (const UniRcFrame& f : frames) dispatch(f);
    }
}

void UniRcSdk::keepAliveLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        if (!running_.load()) break;
        if (!isReceiving(1500)) startChannelStream(streamFreq_);
    }
}

void UniRcSdk::dispatch(const UniRcFrame& f) {
    if (f.cmdId == CMD_RC_CHANNELS && f.data.size() >= 8) {
        size_t n = std::min(f.data.size() / 2, static_cast<size_t>(16));
        {
            std::lock_guard<std::mutex> lk(channelsMutex_);
            for (size_t c = 0; c < n; ++c)
                channels_[c] = s16(f.data.data(), c * 2);
        }
        lastChannelMs_.store(nowMs());
        ChannelListener l;
        { std::lock_guard<std::mutex> lk(listenerMutex_); l = listener_; }
        if (l) {
            std::array<int16_t, 16> snap;
            { std::lock_guard<std::mutex> lk(channelsMutex_); snap = channels_; }
            l(snap);
        }
        return;
    }
    // deliver command replies to any waiter (matched by cmd id, like the Java).
    std::lock_guard<std::mutex> lk(reqMutex_);
    auto it = pending_.find(f.cmdId);
    if (it != pending_.end()) {
        it->second->frame = f;
        it->second->have = true;
        reqCv_.notify_all();
    }
}

// ---------------------------------------------------------------- low level
bool UniRcSdk::send(int cmdId, bool needAck, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lk(writeMutex_);
    if (!transport_.isOpen()) return false;
    std::vector<uint8_t> f = UniRcFrame::build(cmdId, seq_++ & 0xFFFF, needAck, data);
    return transport_.write(f.data(), f.size());
}

bool UniRcSdk::request(int cmdId, const std::vector<uint8_t>& data, int timeoutMs,
                       UniRcFrame& out) {
    auto p = std::make_shared<Pending>();
    {
        std::lock_guard<std::mutex> lk(reqMutex_);
        pending_[cmdId] = p;
    }
    bool sent = send(cmdId, true, data);
    bool ok = false;
    if (sent) {
        std::unique_lock<std::mutex> lk(reqMutex_);
        ok = reqCv_.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                             [&] { return p->have; });
    }
    {
        std::lock_guard<std::mutex> lk(reqMutex_);
        pending_.erase(cmdId);
    }
    if (ok) out = p->frame;
    return ok;
}

bool UniRcSdk::sendRaw(int cmdId, const std::vector<uint8_t>& data, bool needAck) {
    return send(cmdId, needAck, data);
}

// ---------------------------------------------------------------- channel stream
bool UniRcSdk::startChannelStream(int freqCode) {
    streamFreq_ = freqCode;
    std::vector<uint8_t> d{ static_cast<uint8_t>(freqCode) };
    bool ok = true;
    for (int i = 0; i < 3; ++i) ok = send(CMD_RC_CHANNELS, true, d) && ok; // "send three times"
    return ok;
}

bool UniRcSdk::stopChannelStream() {
    std::vector<uint8_t> d{ static_cast<uint8_t>(FREQ_OFF) };
    bool ok = true;
    for (int i = 0; i < 3; ++i) ok = send(CMD_RC_CHANNELS, true, d) && ok;
    return ok;
}

// ---------------------------------------------------------------- typed commands
bool UniRcSdk::getHardwareId(std::string& out) {
    UniRcFrame r;
    if (!request(CMD_HARDWARE_ID, {}, 800, r)) return false;
    std::string s;
    for (uint8_t b : r.data) { if (b == 0) break; s.push_back(static_cast<char>(b)); }
    out = s;
    return true;
}

bool UniRcSdk::getFirmwareVersions(std::array<std::string, 4>& out) {
    UniRcFrame r;
    if (!request(CMD_FIRMWARE_VERSION, {}, 800, r)) return false;
    const std::vector<uint8_t>& d = r.data;
    for (int i = 0; i < 4; ++i) {
        if (static_cast<size_t>(i * 4 + 3) < d.size()) {
            int b1 = d[i * 4 + 1], b2 = d[i * 4 + 2], b3 = d[i * 4 + 3]; // low byte d[i*4] ignored
            out[i] = std::to_string(b3) + "." + std::to_string(b2) + "." + std::to_string(b1);
        } else {
            out[i].clear();
        }
    }
    return true;
}

bool UniRcSdk::getSystemSettings(SystemSettings& out) {
    UniRcFrame r;
    if (!request(CMD_GET_SYS_SETTINGS, {}, 800, r)) return false;
    const std::vector<uint8_t>& d = r.data;
    SystemSettings s;
    if (d.size() >= 1) s.bindingStatus = d[0];
    if (d.size() >= 2) s.com1Baud = d[1];
    if (d.size() >= 3) s.joyType = d[2];
    if (d.size() >= 4) s.rcBatteryX10 = d[3];
    if (d.size() >= 5) s.com2Baud = d[4];
    out = s;
    return true;
}

bool UniRcSdk::setStickMode(int joyType) {
    // 0x17 payload: match, com1Baud, joyType, reserved, com2Baud. Preserve baud.
    SystemSettings cur;
    if (!getSystemSettings(cur)) return false;
    std::vector<uint8_t> d{
        0,
        static_cast<uint8_t>(cur.com1Baud == 0 ? 5 : cur.com1Baud),
        static_cast<uint8_t>(joyType),
        0,
        static_cast<uint8_t>(cur.com2Baud == 0 ? 5 : cur.com2Baud)
    };
    UniRcFrame r;
    if (!request(CMD_SET_SYS_SETTINGS, d, 800, r)) return false;
    return r.data.size() >= 1 && r.data[0] == 1;
}

// ---------------------------------------------------------------- binding
bool UniRcSdk::startBinding() { return setBinding(true); }
bool UniRcSdk::stopBinding()  { return setBinding(false); }

bool UniRcSdk::setBinding(bool start) {
    SystemSettings cur;
    if (!getSystemSettings(cur)) return false;   // preserve baud/joyType
    std::vector<uint8_t> d{
        static_cast<uint8_t>(start ? 1 : 0),
        static_cast<uint8_t>(cur.com1Baud == 0 ? 5 : cur.com1Baud),
        static_cast<uint8_t>(cur.joyType),
        0,
        static_cast<uint8_t>(cur.com2Baud == 0 ? 5 : cur.com2Baud)
    };
    UniRcFrame r;
    if (!request(CMD_SET_SYS_SETTINGS, d, 800, r)) return false;
    return r.data.size() >= 1 && r.data[0] == 1;
}

bool UniRcSdk::getBindingStatus(int& out) {
    SystemSettings s;
    if (!getSystemSettings(s)) return false;
    out = s.bindingStatus;
    return true;
}

// ---------------------------------------------------------------- mapping
bool UniRcSdk::getAllChannelMappings(std::vector<Mapping>& out) {
    UniRcFrame r;
    if (!request(CMD_GET_ALL_MAPPINGS, {}, 800, r)) return false;
    const std::vector<uint8_t>& d = r.data;
    size_t n = d.size() / 2;
    out.clear();
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        Mapping m;
        m.rcChannel = static_cast<int>(i) + 1;
        m.type = d[i * 2];
        m.entityId = d[i * 2 + 1];
        out.push_back(m);
    }
    return true;
}

bool UniRcSdk::setChannelMapping(int rcChannel, int type, int entityId) {
    std::vector<uint8_t> d{
        static_cast<uint8_t>(rcChannel),
        static_cast<uint8_t>(type),
        static_cast<uint8_t>(entityId)
    };
    UniRcFrame r;
    if (!request(CMD_SET_MAPPING, d, 800, r)) return false;
    return r.data.size() >= 2 && r.data[1] == 1;
}

// ---------------------------------------------------------------- reverse
bool UniRcSdk::getAllReverse(std::vector<int>& out) {
    UniRcFrame r;
    if (!request(CMD_GET_ALL_REVERSE, {}, 800, r)) return false;
    const std::vector<uint8_t>& d = r.data;
    out.clear();
    out.reserve(d.size());
    for (uint8_t b : d) out.push_back(static_cast<int>(static_cast<int8_t>(b))); // signed: 1 or -1
    return true;
}

bool UniRcSdk::setChannelReverse(int rcChannel, bool reversed) {
    std::vector<uint8_t> d{
        static_cast<uint8_t>(rcChannel),
        static_cast<uint8_t>(reversed ? -1 : 1)
    };
    UniRcFrame r;
    if (!request(CMD_SET_REVERSE, d, 800, r)) return false;
    return r.data.size() >= 2 && r.data[1] == 1;
}

} // namespace unircsdk
