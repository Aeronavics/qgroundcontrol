# UniRC 7 Pro SDK — C++ port (for QGroundControl)

A faithful **C++14, Qt-independent** port of the hardware-verified Java SDK in
`../java/com/siyi/unircsdk/`. Same command ids, same baud rates, same frame
layout, same gating logic. Depends on **C++ std + POSIX only** — no Qt, no
third-party libraries — so it drops straight into a QGC/Android build where
`QSerialPort` is unavailable.

Namespace: `unircsdk`. Standard `#pragma once`, RAII, `enum class`, `nullptr`.

## Files

| File | What |
|---|---|
| `crc.h` | Inline `crc16_xmodem()` (CRC-16/XMODEM) + `crc8_maxim()` (CRC-8/MAXIM). Header-only. |
| `serial_port.h` / `.cpp` | POSIX serial: `open(dev, baud)` via `open()`+`termios` (raw 8N1, `cfset*speed` with `B230400`/`B115200`, `VMIN=0`/`VTIME=1`), `read`, `write`, `close`. |
| `unirc_frame.h` | 0x5566 frame `build()` + streaming `Parser` + little-endian helpers (`u16`/`s16`/`u32`). Header-only. |
| `rcu_session.h` / `.cpp` | **The internal driver.** RC-MCU link on `/dev/ttyHS1` @230400: session open/keepalive, live channels, all config setters, gated calibration. |
| `unirc_sdk.h` / `.cpp` | External SDK on `/dev/ttyHS3` @115200: channel streaming + synchronous request/response for hw id, firmware, system settings, binding, mappings, reverse. |
| `udp_port.h` / `.cpp` | POSIX UDP socket (unconnected `sendto`/`recvfrom` + `SO_RCVTIMEO`) for the FPV upgrade link. |
| `fpv_frame.h` | The `0xAA` V3 frame (`CMD_ID` + `SUBCMD`) used by the FPV upgrade channel. Header-only. |
| `ftp_upload.h` / `.cpp` | Minimal FTP `STOR` + `SIZE` — where the FPV firmware bytes actually travel. |
| `fpv_upgrade_client.h` / `.cpp` | **Ground-unit FPV firmware upgrade.** See `FIRMWARE_UPGRADE_PROTOCOL.md`. |

## Building

`CMakeLists.txt` builds these as the **`Siyi`** static library, wired into QGC via
`QGC_SIYI_ENABLED` (defaults ON everywhere except Windows). The library is
intentionally **Qt-free** — plain C++14 + POSIX — so Qt's autogen passes are
disabled for the target and it can be driven from a plain `main()` or a test
harness. It links only `Threads::Threads`.

Because it is POSIX-only (termios + BSD sockets) it does **not** build on
Windows; enabling `QGC_SIYI_ENABLED` there is a hard configure error rather than
a pile of compile errors.

To use it standalone (e.g. cross-compiled for the controller to test on-device):
`g++ -std=c++14 -pthread rcu_session.cpp serial_port.cpp your_main.cpp`.

## Two links, two protocols (both verified)

* **Internal — `RcuSession`, `/dev/ttyHS1` @230400, `AA`-framed, session-based.**
  `open()` sends CMD `0x14` to target `0x10`, then a keepalive CMD `0x35` to
  target `0xF0` every 50 ms. Only inside this live session does the MCU stream
  live channels (CMD `0x01`, 16×int16) and accept config/calibration commands.
  TX frame: `AA 09 03 | LEN(2 LE) | CRC8-MAXIM(hdr) | SEQ(2 LE) | D0 10 TARGET | CMD | DATA | CRC16-XMODEM(2 LE)`.
  Only one process may own `ttyHS1` — stop SIYI's `rcuservice`/UniGCS first.

* **External — `UniRcSdk`, `/dev/ttyHS3` @115200, `0x5566`-framed.**
  `start()` opens the port and arms the `0x42` channel stream.
  **Prerequisite:** `ttyHS3` is silent until the RC unit's "Remote control SDK
  connection method" = **UART**. Set it once (persists in the MCU) via
  `RcuSession::setSdkConnectType(RcuSession::SdkConnectType::UART)` or in UniGCS.
  (`UniRcTransport.Udp` from the Java is intentionally omitted — it was
  non-functional on the test unit; serial is the only proven transport.)

## Threading

Both classes own their I/O threads with `std::thread` and use RAII shutdown.

* `RcuSession`
  * **rx thread** (`rxLoop`) — reads the serial port, runs a streaming `AA`
    framer (`feed`), dispatches CMD `0x01` (channels) / `0x04` (calibration
    step) / everything else.
  * **keepalive thread** (`keepAliveLoop`) — sends CMD `0x35` every 50 ms.
  * Writes are serialised by `writeMutex_` (also guards the seq counter).
    Live channels are copied under `channelsMutex_`. `calStep_`,
    `lastChannelMs_`, and `running_` are `std::atomic`. Listeners are
    swapped/read under `listenerMutex_` and always invoked **outside** the
    channel lock.
* `UniRcSdk`
  * **rx thread** — streaming `0x5566` parser; channel frames update state +
    fire the listener, all other frames are delivered to a waiting `request()`.
  * **keepalive thread** — re-arms the channel stream if no data for 1.5 s.
  * **Synchronous request/response** replaces Java's `SynchronousQueue`:
    `request()` registers a `Pending` in a `std::map` keyed by **cmd id**,
    sends, then waits on a `std::condition_variable` with a predicate. The rx
    thread matches the reply **by cmd id** (the reply SEQ is not an echo — this
    mirrors the Java exactly) and notifies. Returns `false` on timeout.

`close()` sets `running_ = false`, joins both threads (the rx read has a
~100 ms `VTIME` timeout so it unblocks on its own), then closes the fd.
Destructors call `close()`.

## Calibration gate (the important gotcha, preserved)

`RcuSession::runCalibration(calTarget, progress, sweepMillis)` drives
`START → hold centre (MEDIAN) → sweep (MAX_MIN) → SAVE → SUCCESS/FAILED`.

The reply to `START` is **also step 3** — byte-identical to `SUCCESS`. A naive
client would "succeed" instantly. The state machine therefore **gates**: it will
not accept `SUCCESS(3)`/`FAILED(4)` until `MAX_MIN(2)` has actually been
observed. This is ported verbatim from the Java (`sawMaxMin` guard).

`calTarget`: `CAL_JOYSTICKS`(0x01, all three sticks incl. the HAT nub) or
`CAL_DIALS`(0x02). Deadzone is clamped to 10–80. Returns the final step
(3=SUCCESS, 4=FAILED); `progress` fires on each transition.

## QGC integration note

Wrap each driver in a thin `QObject` and forward the `std::function` callbacks
to Qt signals. The callbacks fire on the SDK's own worker thread, so cross the
thread boundary with a queued connection:

```cpp
class RcuBridge : public QObject {
    Q_OBJECT
public:
    RcuBridge() {
        rcu_.setChannelListener([this](const std::array<int16_t,16>& ch){
            std::array<int16_t,16> copy = ch;
            // marshal onto the Qt thread; the lambda runs on the rx thread
            QMetaObject::invokeMethod(this, [this, copy]{ emit channels(copy); },
                                      Qt::QueuedConnection);
        });
    }
    bool open() { return rcu_.open(); }
signals:
    void channels(std::array<int16_t,16> ch);   // register with qRegisterMetaType
private:
    unircsdk::RcuSession rcu_;
};
```

`UniRcSdk` wraps the same way. Because the transport is plain POSIX
`open()`/`termios`/`read()`/`write()`, it works on **Android** (the UniRC 7 Pro's
OS) where `QSerialPort` does not — which is the whole reason for the POSIX layer.
Call the blocking `request()`/`runCalibration()` helpers from a worker thread (or
`QtConcurrent::run`), never the GUI thread.
