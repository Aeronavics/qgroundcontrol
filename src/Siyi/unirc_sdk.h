#pragma once
//
// UniRcSdk — SIYI UniRC 7 Pro External SDK over /dev/ttyHS3 @115200 (0x5566
// protocol). Read sticks/buttons and configure the RC MCU from your own app.
//
//   UniRcSdk sdk;                                  // /dev/ttyHS3
//   sdk.setChannelListener([](const std::array<int16_t,16>& ch){ ... });
//   sdk.start();                                   // arms the stream
//   std::string hw; sdk.getHardwareId(hw);
//   std::vector<UniRcSdk::Mapping> map; sdk.getAllChannelMappings(map);
//   sdk.setChannelReverse(1, true);
//   sdk.close();
//
// PREREQUISITE: /dev/ttyHS3 is completely silent unless the RC unit's "Remote
// control SDK connection method" = UART. Set it once via
// RcuSession::setSdkConnectType(SdkConnectType::UART) (persists in the MCU), or
// via UniGCS. Without it, start() opens the port but no data ever arrives.
//
// The internal config + hardware stick calibration live in RcuSession (ttyHS1).
//
// Faithful C++14 port of UniRcSdk.java. std + POSIX only; no Qt. Synchronous
// request/response replaces SynchronousQueue with mutex + condition_variable +
// a pending map keyed by cmd id (replies are matched by CMD, not SEQ, exactly
// like the Java).
//
#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <map>
#include <memory>

#include "serial_port.h"
#include "unirc_frame.h"

namespace unircsdk {

class UniRcSdk {
public:
    // ---- command ids (External SDK, from the SDK Guide) ----
    static constexpr int CMD_GET_SYS_SETTINGS  = 0x16;
    static constexpr int CMD_SET_SYS_SETTINGS  = 0x17;
    static constexpr int CMD_HARDWARE_ID       = 0x40;
    static constexpr int CMD_RC_CHANNELS       = 0x42;
    static constexpr int CMD_RC_LINK_INFO      = 0x43;
    static constexpr int CMD_VIDEO_LINK_INFO   = 0x44;
    static constexpr int CMD_FIRMWARE_VERSION  = 0x47;
    static constexpr int CMD_GET_ALL_MAPPINGS  = 0x48;
    static constexpr int CMD_GET_MAPPING       = 0x49;
    static constexpr int CMD_SET_MAPPING       = 0x4A;
    static constexpr int CMD_GET_ALL_REVERSE   = 0x4B;
    static constexpr int CMD_GET_REVERSE       = 0x4C;
    static constexpr int CMD_SET_REVERSE       = 0x4D;
    static constexpr int CMD_MULTI_LINK_STATUS = 0x4E;
    static constexpr int CMD_SYSTEM_STATUS     = 0x4F;

    // 0x42 output frequency codes.
    static constexpr int FREQ_OFF = 0, FREQ_2HZ = 1, FREQ_4HZ = 2, FREQ_5HZ = 3,
                         FREQ_10HZ = 4, FREQ_20HZ = 5, FREQ_50HZ = 6, FREQ_100HZ = 7;

    // Binding status (from 0x16 `match`): 0 idle, 1-2 in progress, 3 complete.
    static constexpr int BIND_IDLE = 0, BIND_IN_PROGRESS_1 = 1,
                         BIND_IN_PROGRESS_2 = 2, BIND_COMPLETE = 3;

    // Physical-control category codes for Mapping::type (verified on hardware).
    static constexpr int PHY_STICK_WHEEL = 0, PHY_BUTTON = 1, PHY_VIRTUAL = 2,
                         PHY_KNOB = 4, PHY_SWITCH_3POS = 5, PHY_PPM = 6;
    static std::string physName(int type);

    static constexpr const char* DEFAULT_PORT = "/dev/ttyHS3";
    static constexpr int BAUD = 115200;

    using ChannelListener = std::function<void(const std::array<int16_t, 16>&)>;

    // One output channel's source: which physical control (type + entityId)
    // drives rcChannel.
    struct Mapping {
        int rcChannel = 0;
        int type = 0;
        int entityId = 0;
        std::string toString() const;
        std::string name() const;
    };

    struct SystemSettings {
        int bindingStatus = 0;
        int com1Baud = 0;
        int joyType = 0;
        int rcBatteryX10 = 0;
        int com2Baud = 0;
        float rcBatteryVolts() const { return rcBatteryX10 / 10.0f; }
        std::string toString() const;
    };

    explicit UniRcSdk(std::string port = DEFAULT_PORT) : port_name_(std::move(port)) {}
    ~UniRcSdk() { close(); }

    UniRcSdk(const UniRcSdk&) = delete;
    UniRcSdk& operator=(const UniRcSdk&) = delete;

    void setChannelListener(ChannelListener l);
    std::array<int16_t, 16> channels();
    bool isReceiving(long withinMs);

    // ---- lifecycle ----
    bool start();     // open port, start rx + keepalive threads, arm the stream
    void close();
    bool isOpen() const { return running_.load(); }

    // ---- channel stream ----
    bool startChannelStream(int freqCode);
    bool stopChannelStream();

    // ---- low level ----
    // Send a command and wait for the reply frame with the same cmd id.
    // Returns false on timeout; the reply (on success) is written to `out`.
    bool request(int cmdId, const std::vector<uint8_t>& data, int timeoutMs, UniRcFrame& out);
    // Escape hatch: send any raw command.
    bool sendRaw(int cmdId, const std::vector<uint8_t>& data, bool needAck);

    // ---- typed commands (return false on timeout / bad reply) ----
    bool getHardwareId(std::string& out);
    bool getFirmwareVersions(std::array<std::string, 4>& out);
    bool getSystemSettings(SystemSettings& out);
    bool setStickMode(int joyType);

    // ---- binding (RC <-> air unit) ----
    bool startBinding();
    bool stopBinding();
    bool getBindingStatus(int& out);

    // ---- channel mapping ----
    bool getAllChannelMappings(std::vector<Mapping>& out);
    bool setChannelMapping(int rcChannel, int type, int entityId);

    // ---- channel reverse (false=normal(1), true=reversed(-1)) ----
    bool getAllReverse(std::vector<int>& out);
    bool setChannelReverse(int rcChannel, bool reversed);

private:
    void rxLoop();
    void keepAliveLoop();
    void dispatch(const UniRcFrame& f);
    bool send(int cmdId, bool needAck, const std::vector<uint8_t>& data);
    bool setBinding(bool start);

    static long long nowMs();

    struct Pending {
        bool have = false;
        UniRcFrame frame;
    };

    std::string port_name_;
    SerialPort transport_;
    UniRcFrame::Parser parser_;

    std::atomic<bool> running_{false};
    std::thread rxThread_;
    std::thread keepAlive_;

    std::mutex writeMutex_;   // serialises send() and the seq counter
    int seq_ = 0;
    int streamFreq_ = FREQ_20HZ;

    std::mutex channelsMutex_;
    std::array<int16_t, 16> channels_{};
    std::atomic<long long> lastChannelMs_{0};

    std::mutex listenerMutex_;
    ChannelListener listener_;

    std::mutex reqMutex_;
    std::condition_variable reqCv_;
    std::map<int, std::shared_ptr<Pending>> pending_;
};

} // namespace unircsdk
