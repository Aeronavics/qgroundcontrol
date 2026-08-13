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
//   open (0x14) + keepalive (0x35) is the WHOLE handshake for CMD_CHANNELS
//   and ordinary config GET/SET — confirmed still true (2026-08-13): flight
//   mode/channel/deadzone GETs and calibration all work over a cold-boot
//   session with no UniGCS involvement. BUT CMD_ANALOG_RAW (0x3E) is its own
//   exception - verified by isolated capture (2026-08-13) to sit completely
//   silent after a fresh boot until something arms it, and once armed it
//   stays armed across app restarts (only a real reboot disarms it again).
//   Found the trigger by testing raw commands directly: CMD_ANALOG_RAW
//   follows the exact same request/reply-share-the-same-id pattern as
//   UniRcSdk::startChannelStream() on the External SDK - send the cmd id
//   itself with a one-byte payload (3x, same "send three times" idiom) to
//   start it, 0 to stop it. Unlike that External SDK command, the payload
//   isn't a rate selector here (tested and disproved) - just on/off. See
//   startAnalogStream()/stopAnalogStream().
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
    // MCU -> app raw analog input stream, 12x int16 LE, small signed range
    // (observed roughly -100..100), independent of channel mapping and always
    // live (not gated by calibration state) - this is what UniGCS's
    // calibration screens actually read for their live crosshairs, NOT
    // CMD_CALIBRATION (0x04, which carries no position data at all - see the
    // ANALOG_* slot indices below). Reverse-engineered by isolated capture
    // (2026-08-12): each physical stick/dial/HAT axis wiggled alone, one at a
    // time, and matched to whichever of the 12 slots moved.
    static constexpr int CMD_ANALOG_RAW = 0x3E;
    // Confirmed slot -> physical control mapping (isolated single-axis
    // captures; slots 6,7,10,11 never carried real movement - 6 was flat
    // low-magnitude noise, 10/11 were single-frame outliers with no smooth
    // ramp in/out, most likely CRC-16 coincidence on a corrupted byte rather
    // than real data, and 7 never moved at all).
    static constexpr int ANALOG_J1           = 0;
    static constexpr int ANALOG_J2           = 1;
    static constexpr int ANALOG_J3           = 2;
    static constexpr int ANALOG_J4           = 3;
    static constexpr int ANALOG_LD           = 4;
    static constexpr int ANALOG_RD           = 5;
    static constexpr int ANALOG_HAT_VERTICAL   = 8;
    static constexpr int ANALOG_HAT_HORIZONTAL = 9;
    // One-byte payload for CMD_ANALOG_RAW as a request (see
    // startAnalogStream()): 0 stops the stream, any nonzero value starts it.
    // Initially assumed (by analogy with UniRcSdk::FREQ_* on the External
    // SDK, which shares the small 0-7 value range) that nonzero values
    // select a delivery rate - tested directly on hardware (2026-08-13) and
    // disproved: values 1, 5, and 7 all measured the same ~38Hz. So this
    // parameter isn't a rate selector here, just on/off; the value used
    // below is arbitrary among the nonzero ones that were tested.
    static constexpr int ANALOG_STREAM_OFF = 0;
    static constexpr int ANALOG_STREAM_ON  = 5;

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
    using AnalogListener  = std::function<void(const std::array<int16_t, 12>&)>;
    using FrameListener   = std::function<void(int cmd, const std::vector<uint8_t>&)>;
    using CalProgress     = std::function<void(int step)>;

    explicit RcuSession(std::string port = PORT) : port_name_(std::move(port)) {}
    ~RcuSession() { close(); }

    RcuSession(const RcuSession&) = delete;
    RcuSession& operator=(const RcuSession&) = delete;

    void setChannelListener(ChannelListener l);
    void setAnalogListener(AnalogListener l);
    void setFrameListener(FrameListener l);

    // Snapshot of the latest 16 live channel values (mutex-guarded copy).
    std::array<int16_t, 16> channels();
    // Snapshot of the latest 12 raw analog values (CMD_ANALOG_RAW) - see the
    // ANALOG_* constants above for which slot is which physical control.
    std::array<int16_t, 12> analogRaw();
    bool isReceiving(long withinMs);

    // Arms/disarms the CMD_ANALOG_RAW stream - see the class comment above
    // for how this was found. Sends the cmd id itself 3x as the request,
    // matching UniRcSdk::startChannelStream()'s "send three times" idiom on
    // the External SDK. open() calls this automatically; call again
    // explicitly only if you need to stop it.
    bool startAnalogStream(int payload = ANALOG_STREAM_ON);
    bool stopAnalogStream() { return startAnalogStream(ANALOG_STREAM_OFF); }

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
    std::array<int16_t, 12> analogRawCopy();

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

    std::mutex analogMutex_;
    std::array<int16_t, 12> analog_{};     // value-initialised to zero

    std::mutex listenerMutex_;
    ChannelListener channelListener_;
    AnalogListener analogListener_;
    FrameListener frameListener_;

    std::atomic<int> calStep_{0};

    std::vector<uint8_t> acc_;     // rx streaming framer buffer
};

} // namespace unircsdk
