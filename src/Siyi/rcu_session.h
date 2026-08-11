#pragma once
//
// RcuSession — driver for the UniRC 7 Pro's INTERNAL RC-unit link on
// /dev/ttyHS1. This is the link UniGCS's rcuservice normally owns; driving it
// ourselves lets the app do calibration / failsafe / all RCU config WITHOUT
// UniGCS running.
//
// Session (reverse-engineered from rcuservice's WriteTask handshake):
//   1. open the link with CMD 0x14 (target 0x10 = RC MCU)
//   2. keep it alive with periodic CMD 0x35 (target 0xF0) every ~50ms
//   3. the MCU then streams live channels as CMD 0x01 (16x int16, ~1050..1950)
//      and answers config/calibration commands in the same stream.
//
// Frame format (internal AA protocol):
//   send: AA 09 03 | LEN(2LE) | HCRC8 | SEQ(2LE) | D0 10 TARGET | CMD | DATA | CRC16
//   recv: AA 0A 03 | LEN(2LE) | HCRC8 | SEQ(2LE) | 10 D0 10    | CMD | DATA | CRC16
// HCRC8 = CRC-8/MAXIM over the 5 header bytes; CRC16 = CRC-16/XMODEM over the frame.
//
// NOTE: only one process may own ttyHS1 — stop/uninstall UniGCS (rcuservice)
// first, or reads collide.
//
// HANDSHAKE / "must open UniGCS first" (verified cold-boot, 2026-08-06):
//   open (0x14) + keepalive (0x35) is the WHOLE handshake — there is no extra
//   "start streaming" command, and UniGCS is NOT required for this internal link.
//   BUT /dev/ttyHS1 comes up at 9600 baud on a fresh boot; you MUST set 230400
//   (open() does this). An app that skips the baud set reads half-rate garble
//   until UniGCS's native init has run once — the usual "open UniGCS first" symptom.
//   (The External SDK on ttyHS3/UDP is separately gated by SDK-connect-type 0x82.)
//   Full analysis + the complete command catalog: ../RCU_INTERNAL_PROTOCOL.md.
//
// GET queries: send the (odd) GET cmd with an empty payload; the MCU replies on
//   the same id — read it via setFrameListener(). SET is the paired even id.
//
// Faithful C++14 port of RcuSession.java. std + POSIX only; no Qt.
//
#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

#include "serial_port.h"

namespace unircsdk {

class RcuSession {
public:
    static constexpr const char* PORT = "/dev/ttyHS1";
    static constexpr int BAUD = 230400;

    // ---- targets ----
    static constexpr int TARGET_RC_MCU = 0x10;
    static constexpr int TARGET_IMAGE  = 0x14;
    static constexpr int TARGET_F0     = 0xF0;   // debug/keepalive channel

    // ---- session command ids ----
    static constexpr int CMD_OPEN      = 0x14;   // open/init the link
    static constexpr int CMD_KEEPALIVE = 0x35;   // request-debug-info (keepalive)
    static constexpr int CMD_CHANNELS  = 0x01;   // MCU -> app live channel data

    // ---- config command ids (verified byte-perfect vs UniGCS) ----
    static constexpr int CMD_CALIBRATION     = 0x04;
    static constexpr int CMD_FAILSAFE        = 0x1B;
    static constexpr int CMD_CHANNEL15       = 0x17;
    static constexpr int CMD_BUTTON_MODE     = 0x32;
    static constexpr int CMD_DEADZONE        = 0x54;
    static constexpr int CMD_FLIGHT_MODE     = 0x62;
    static constexpr int CMD_FLIGHT_CHANNEL  = 0x64;   // set which comm channel the flight mode drives
    static constexpr int CMD_SET_SDK_CONNECT = 0x82;
    static constexpr int CMD_CHANNEL_ADAPTION= 0x72;

    // ---- GET/query command ids (send with EMPTY payload; MCU replies on the same
    //      cmd id with the value). Pattern across the RCU config space: GET = odd,
    //      SET = GET+1. Replies arrive through the FrameListener / dispatch(). ----
    static constexpr int CMD_GET_DEADZONE       = 0x53;  // reply [value]                 (SET 0x54)
    static constexpr int CMD_GET_FLIGHT_MODE    = 0x61;  // reply [mode 0/1/2]            (SET 0x62)
    static constexpr int CMD_GET_FLIGHT_CHANNEL = 0x63;  // reply [channel 1..16]         (SET 0x64)
    static constexpr int CMD_GET_SDK_CONNECT    = 0x81;  // reply [type]                  (SET 0x82)
    static constexpr int CMD_GET_BUTTON_MODE    = 0x34;  // reply list of {keyId, mode}   (SET 0x32 per-key)
    static constexpr int CMD_GET_FAILSAFE       = 0x19;  // reply [enabled, 16x{mode,PWM_LE16}] (master SET 0x1B)
    static constexpr int CMD_GET_CHANNEL_MAP    = 0x0C;  // reply 16x{type, entity}       (also External SDK 0x48)
    static constexpr int CMD_GET_CHANNEL_ADAPTION = 0x71; // target 0x14; reply [enabled, channel] (SET 0x72)

    // ---- calibration constants ----
    static constexpr int CAL_JOYSTICKS = 0x01;
    static constexpr int CAL_DIALS     = 0x02;
    static constexpr int CAL_START     = 0x01;
    static constexpr int CAL_POLL      = 0x00;
    static constexpr int CAL_SAVE      = 0x02;

    // Physical button ids for setButtonMode().
    static constexpr int BTN_S1 = 0, BTN_S2 = 1, BTN_S3 = 2, BTN_S4 = 3;

    // ---- enums (underlying int values match the wire bytes) ----
    enum class ButtonMode : int { MOMENTARY = 0, LOCK = 1, SWITCH = 2 };
    enum class FlightMode : int { OFF = 0, GEAR3 = 1, GEAR6 = 2 };
    enum class SdkConnectType : int { CLOSE = 0, BLUETOOTH = 1, TYPEC = 2, UART = 3, UDP = 4 };
    enum class Channel15Mode : int { SEARCHLIGHT = 0, GIMBAL_A2MINI = 1 };
    enum class Step : int { NORMAL = 0, MEDIAN = 1, MAX_MIN = 2, SUCCESS = 3, FAILED = 4 };

    // ---- callbacks ----
    using ChannelListener = std::function<void(const std::array<int16_t, 16>&)>;
    using FrameListener   = std::function<void(int cmd, const std::vector<uint8_t>&)>;
    using CalProgress     = std::function<void(int step)>;

    explicit RcuSession(std::string port = PORT) : port_name_(std::move(port)) {}
    ~RcuSession() { close(); }

    RcuSession(const RcuSession&) = delete;
    RcuSession& operator=(const RcuSession&) = delete;

    void setChannelListener(ChannelListener l);
    void setFrameListener(FrameListener l);

    // Snapshot of the latest 16 live channel values (mutex-guarded copy).
    std::array<int16_t, 16> channels();
    bool isReceiving(long withinMs);

    // Configure 230400/8N1, send open (0x14), start rx + keepalive threads.
    // Returns true on success.
    bool open();
    void close();
    bool isOpen() const { return running_.load(); }

    // ---- low level send / frame builder ----
    // Send a command into the live session (keeps the fd open). Returns false if
    // the session is not open or the write failed. Serialised by an internal mutex.
    bool send(int target, int cmd, const std::vector<uint8_t>& data);
    bool send(int cmd, const std::vector<uint8_t>& data) { return send(TARGET_RC_MCU, cmd, data); }

    // Build an AA app->MCU frame. NOTE: increments the sequence counter; call
    // through send() (which holds the write lock) rather than concurrently.
    std::vector<uint8_t> buildFrame(int target, int cmd, const std::vector<uint8_t>& data);

    // ---- RCU config (through the live session) ----
    void setFailsafeEnabled(bool en);
    void setDeadzone(int v);                       // clamped to 10..80
    void setFlightMode(int m);
    void setFlightMode(FlightMode m) { setFlightMode(static_cast<int>(m)); }
    void setFlightChannel(int ch);                 // ch = 1..16 (clamped); cmd 0x64
    void setSdkConnectType(int t);
    void setSdkConnectType(SdkConnectType t) { setSdkConnectType(static_cast<int>(t)); }
    void setChannel15Mode(int m);
    void setChannel15Mode(Channel15Mode m) { setChannel15Mode(static_cast<int>(m)); }
    void setButtonMode(int buttonId, int mode);
    void setButtonMode(int buttonId, ButtonMode mode) { setButtonMode(buttonId, static_cast<int>(mode)); }
    void setChannelAdaption(bool en, int ch);

    // ---- GET / query ----
    // Fire a query: send the GET cmd with an empty payload. The MCU answers on the
    // same cmd id; register a FrameListener (setFrameListener) to receive the reply.
    // target defaults to the RC MCU (0x10); pass TARGET_IMAGE for 0x14 queries
    // (e.g. CMD_GET_CHANNEL_ADAPTION).
    void requestGet(int getCmd, int target = TARGET_RC_MCU);
    void getFlightMode()      { requestGet(CMD_GET_FLIGHT_MODE); }
    void getFlightChannel()   { requestGet(CMD_GET_FLIGHT_CHANNEL); }
    void getDeadzone()        { requestGet(CMD_GET_DEADZONE); }
    void getSdkConnectType()  { requestGet(CMD_GET_SDK_CONNECT); }
    void getButtonModes()     { requestGet(CMD_GET_BUTTON_MODE); }
    void getFailsafe()        { requestGet(CMD_GET_FAILSAFE); }
    void getChannelMap()      { requestGet(CMD_GET_CHANNEL_MAP); }
    void getChannelAdaption() { requestGet(CMD_GET_CHANNEL_ADAPTION, TARGET_IMAGE); }

    // ---- calibration (poll-driven state machine) ----
    // Live step reported in the last 0x04 reply's data[1].
    int calStep() const { return calStep_.load(); }
    void calStart(int calTarget);
    void calPoll(int calTarget);
    void calSave(int calTarget);

    // Full interactive calibration: START -> poll to MEDIAN (hold centre) ->
    // poll to MAX_MIN (sweep for sweepMillis) -> SAVE -> poll to SUCCESS/FAILED.
    // Returns the final step (3=SUCCESS, 4=FAILED). cb fires on each transition.
    //
    // GATE: the START-ack is ALSO step 3, so SUCCESS(3)/FAILED(4) are ignored
    // until MAX_MIN(2) has actually been observed. Ported verbatim.
    int runCalibration(int calTarget, CalProgress cb, long sweepMillis);

private:
    void rxLoop();
    void keepAliveLoop();
    void feed(const uint8_t* data, size_t n);
    void dispatch(int cmd, const std::vector<uint8_t>& d);
    std::array<int16_t, 16> channelsCopy();

    static long long nowMs();
    static void sleepMs(long ms);

    std::string port_name_;
    SerialPort port_;

    std::atomic<bool> running_{false};
    std::thread rxThread_;
    std::thread kaThread_;
    long keepAliveMs_ = 50;

    std::mutex writeMutex_;       // serialises send() and the seq counter
    int seq_ = 0;

    std::mutex lifecycleMutex_;   // guards open()/close()

    std::mutex channelsMutex_;
    std::array<int16_t, 16> channels_{};   // value-initialised to zero
    std::atomic<long long> lastChannelMs_{0};

    std::mutex listenerMutex_;
    ChannelListener channelListener_;
    FrameListener frameListener_;

    std::atomic<int> calStep_{0};

    std::vector<uint8_t> acc_;     // rx streaming framer buffer
};

} // namespace unircsdk
