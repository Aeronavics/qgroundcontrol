# SIYI UniRC7 Pro — ground unit FPV (image-transmission) firmware upgrade protocol

Reverse-engineered from a decompile of **UniGCS 3.1.4** (`biz.siyi.remotecontrol`, pulled live
off this UniRC7 Pro via `adb`) **and** live `adb logcat` captures of two real upgrade attempts on
this hardware. Companion to `README_cpp.md` / `SDK_REFERENCE.md` / `CAPTURE_PLAYBOOK.md`, which
cover the joystick/calibration SDK already ported into `rcu_session.cpp`. This doc covers a
different subsystem entirely: **the ground unit's own video/telemetry-link ("FPV") module
firmware** — not the gimbal/camera, and not the RC handheld's own MCU.

Confidence key: ✅ **live-captured** — real bytes off this hardware, both directions confirmed
where noted. 🟡 decompiled-only / not yet exercised on the wire.

---

## 0. Four upgrade subsystems on this one screen — don't confuse them

UniGCS's "Device Info" screen offers **four** separate upgrade targets
(`biz/siyi/pilot/bu/rcu/ui/deviceinfo/w2.java`: `GROUND_MCU`, `SKY_MCU`, `GROUND_IMAGE`,
`SKY_IMAGE`), plus a fifth, unrelated gimbal/camera path elsewhere in the app:

1. **Gimbal/camera firmware** (`biz/siyi/camera/upgrade/*.java`, `CameraUpgradeConfig.java`) —
   targets `q.CAMERA`(52)/`q.GIMBAL`(46), UDP to `192.168.144.25:37280` / `.60:37280`. **Not this doc.**
2. **`GROUND_MCU` / `SKY_MCU`** — the RC handheld's own MCU and the aircraft's RC *receiver*
   module. Goes over the internal `AA`-framed serial link (`/dev/ttyHS1`, group `240`/0xF0,
   cmd `32`/`35`/`38`) — the same family `RcuSession` already implements for calibration. **Not
   this doc** (documented briefly in §5 for comparison/because it's easy to confuse with the
   real target).
3. **`GROUND_IMAGE` / `SKY_IMAGE`** — the actual FPV video+telemetry datalink module firmware.
   **This doc.** Confirmed by live capture to go over **UDP**, not the serial link.

---

## 1. Entry point (confirmed)

Device Info screen → pick `GROUND_IMAGE` → pick a local `.bin`/`.zip` firmware file → tap
Upgrade. `biz/siyi/pilot/bu/rcu/ui/deviceinfo/o0.java:36-65` dispatches on the `w2` selector; for
`GROUND_IMAGE`/`SKY_IMAGE` it launches `biz/siyi/pilot/bu/rcu/viewmodel/j1.java`, which (for
ground) calls straight into `DeviceInfoViewModel.j()`'s UDP client — no relay/wait prelude for
ground (that 12 s relay-enable wait is `SKY_IMAGE`-only in the decompile, see §5).

The firmware file is **user-supplied from local storage** — there is no server manifest/URL for
this path (unlike the gimbal/camera path's `config.json`). Before uploading, UniGCS compares the
filename's prefix (substring before the first `_`) against a hardware-model string it queries
live from the connected unit, and refuses with `R.string.rcu_hardware_model_wrong_tip` on
mismatch (`biz/siyi/pilot/bu/rcu/viewmodel/y.java:55-62`).

---

## 2. Transport — ✅ confirmed live: **UDP, port 37250**

**Correction from an earlier draft of this doc:** the ground-unit FPV firmware transfer does
**not** go over `/dev/ttyHS1`. It goes over **UDP** to the ground unit's IP, port **37250**.
Live-captured (`adb logcat`, clean run, no port contention):

```
D/WriteTask( 5349): write data, data: AA090328004E0000D01E6F02435836363533432D4E5F41352E312E322E362D73766E37352D323032362D30362D30382E7A6970311609
```
decodes to:
```
STX=0xAA CTRL=0x09 VER=0x03 DATALEN=40 HCRC8=0x4E SEQ=0 TARGET=0xD0 SOURCE=0x1E CMD_ID=111 SUBCMD=2
DATA(40B) = "CX6653C-N_A5.1.2.6-svn75-2026-06-08.zip" + 1 trailing byte 0x31
CRC16 = 0x1609 (LE: 09 16)
```

This is the **same "WriteTask" logcat tag** as the `AA`-framed RCU serial protocol, because both
sit on the same `SIYIBaseProtocolWRer` base class (`biz/siyi/protocol/bu/manufacturer/siyi/SIYIBaseProtocolWRer.java`)
— logging happens above the transport, which is why capturing this doesn't require a raw serial
tap, just `adb logcat`. The socket itself is `biz.siyi.port.m`, tagged `"UdpPortCommunication"`
in its own logs — a plain `java.net.DatagramSocket`, **no TCP, no FTP** despite an internal enum
literally named `h4.a.FTP` (that's just a "use the chunked handshake" mode label, not the wire
protocol).

### Frame layout (all fields confirmed against the captured bytes above)

| Offset | Field | Size | Notes |
|---|---|---|---|
| 0 | STX | 1 | always `0xAA` |
| 1 | CTRL | 1 | `0x09` observed (need_ack bit set) |
| 2 | VER | 1 | `0x03` (V3 — 2-byte length field, CMD_ID present) |
| 3-4 | DATA_LEN | 2 LE | length of DATA only |
| 5 | HCRC8 | 1 | CRC-8 over bytes `[0..4]` (table-driven, same family as `Crc8.java`) |
| 6-7 | SEQ | 2 LE | auto-increment per connection, wraps at 65536 |
| 8 | TARGET | 1 | `0xD0` = `q.ParamAdTool`(208) |
| 9 | SOURCE | 1 | `0x1E` = `q.Transmission`(30) — i.e. this addresses the image-transmission module |
| 10 | CMD_ID | 1 | **`111`(0x6F)** for every upgrade-related frame |
| 11 | SUBCMD | 1 | see table below |
| 12..12+n | DATA | n = DATA_LEN | payload |
| last 2 | CRC16 | 2 LE | table-driven CRC16 over the whole frame |

### Target IP

Default `192.168.144.12` (`DeviceInfoViewModel.j()`, `DeviceInfoViewModel.java:753-766`),
overridden by a live-queried IP if `GET_IMAGE_TRANS_IP` (RCU-serial group `20`/0x14, cmd
`117`/0x75, **not observed sent in either capture** — see §4) has already returned one.

---

## 3. SUBCMD table

| SUBCMD | Direction | Purpose | Payload | Status |
|---|---|---|---|---|
| **2** | app→unit | Start upload / send filename | 40-byte fixed field: ASCII filename, see note below | ✅ send bytes confirmed live (above); ack **not yet captured** — see §6 |
| 3 | app→unit | Write chunk | `[fileId_lo, fileId_hi] + chunk bytes` (per decompile, `k.java:73-93`) | 🟡 not reached in either capture |
| 4 | app→unit | Finish / commit | 4-byte fileLen (LE) + 32 ASCII bytes of MD5 **hex string** (not raw digest) (`f.java:38-53`) | 🟡 not reached |
| 5 | app→unit | Confirm/apply (reboot into new firmware) | none | 🟡 not reached |
| 6 | app→unit | Query hardware/version string | none → ASCII reply | 🟡 not reached |

Status codes (`m5.d` enum, same values used by both this UDP path and the unrelated `GROUND_MCU`/
`SKY_MCU` serial path): `OK=1, PACK_REPEAT=3, ERR_PACK_NUM=-1, ERR_FILE=-3, ERR_CHECK_SUM=-7,
ERR_WRITE_FILE_SIZE=-10, ERR_FILE_SIZE=-13, ERR_CONFIG_VERSION_MISMATCH=-15`.

**Filename field oddity (unresolved):** the captured 40-byte DATA is exactly the picked file's
name (`CX6653C-N_A5.1.2.6-svn75-2026-06-08.zip`, 39 chars) plus **one trailing byte `0x31`
(`'1'`)** that was **constant** across all 10 retries (i.e. not a counter — the real per-attempt
counter is the SEQ field). Before wiring this into QGC, confirm whether the real firmware
filename genuinely ends in something after `.zip`, or whether this is a stray byte from a
fixed-40-byte buffer in UniGCS itself (e.g. an off-by-one in however it null-pads/reuses that
buffer). Don't copy the trailing byte blindly.

**Retry behaviour (confirmed):** SUBCMD 2 was sent **10 times, ~2 s apart, SEQ 0→9**, byte-identical
payload each time, before UniGCS gave up — i.e. it retries on no-reply, doesn't escalate/change
payload, and caps out around 10 attempts (~18 s) before failing.

---

## 4. Why we haven't captured SUBCMD 3/4/5/6 yet: this needs `192.168.144.x` to actually be up

Both live-capture attempts on this unit failed **before any reply ever arrived** on the UDP
channel — confirmed root cause, not a guess: at the time of both captures, this UniRC7's only
network interface (`wlan0`) was joined to an ordinary WiFi network (not `192.168.144.x`), and
`dumpsys connectivity` showed no second network/interface for the radio/video module's internal
link at all. Every SUBCMD-2 packet had nowhere to arrive. This is **not** a QGC/protocol problem
— it's a precondition for testing: **the ground unit's own radio/video module needs to have
brought up its `192.168.144.x` side before any upgrade attempt (ground *or* sky) can get a
reply.** Confirm this is up (e.g. `adb shell ip addr show` should show an interface with a
`192.168.144.x` address, or `GET_IMAGE_TRANS_IP` over the RCU serial link should return non-loopback
IPs) before the next capture attempt.

**Capture method note:** use `adb logcat` only — **do not** also run a raw `cat /dev/ttyHS1`
capture in parallel while testing this. We confirmed empirically that doing so competes with
UniGCS's own read of that serial port (a tty only delivers each byte to one reader) and caused a
spurious failure on the first attempt — contradicts `CAPTURE_PLAYBOOK.md`'s "read-only-safe"
claim for timing-sensitive flows like this one; that file should probably be caveated.

Also observed in both captures, seemingly independent of ground/sky selection (contrary to what a
pre-capture read of the decompile suggested — flagging the discrepancy rather than asserting
either reading is right): a short RCU-serial exchange, group `20`(0x14), `SUBCMD 116`(0x74) then
`115`(0x73), replies `01` then `00` respectively, followed by an error Toast. Whether this is a
genuine ground-path precondition check or incidental Device-Info-screen polling is unresolved.

---

## 5. `GROUND_MCU`/`SKY_MCU` (adjacent, not this doc's target — for disambiguation only)

Confirmed present, over the RCU-serial link (`/dev/ttyHS1`, group `240`/0xF0):

| CMD | Name | Payload |
|---|---|---|
| `32`(0x20) | UPGRADE_START (recv ack) | `[ok:u8, totalPackets:u16 LE]` |
| `35`(0x23) | UPGRADE_TRANS (send+ack) | send `[seq:u16 LE]+chunk`; ack `[status:i8, seq:u16 LE]` |
| `38`(0x26) | UPGRADE_END (recv ack) | `[status:i8]` |

Target device-type byte selects which MCU: `q.RCU`(16) for the handheld's own MCU, `q.Receiver`(19)
for the aircraft's RC receiver module. **Neither of these is the FPV video/telemetry link** — do
not reuse this path for `GROUND_IMAGE`/`SKY_IMAGE`. (`biz/siyi/core/rcu/controller/s.java:182`,
`r.java`.)

---

## 6. To finish nailing down SUBCMD 3/4/5/6 (once `192.168.144.x` is confirmed up)

```bash
adb logcat -c
adb logcat -v time > cap_logcat.txt &
# do NOT also cat /dev/ttyHS1 in parallel — see §4
# in UniGCS: Device Info -> GROUND_IMAGE -> pick .bin -> Upgrade -> let it run a few seconds -> cancel
```
then filter `cap_logcat.txt` for `WriteTask` (both directions log through it) and decode with the
frame layout in §2 — same method used to get the confirmed SUBCMD-2 bytes above. Watch
specifically for: the SUBCMD-2 **ack** (should carry a `fileId`), the first SUBCMD-3 chunk (chunk
size isn't confirmed — the sibling `n5/b.java` gives a `170`-byte ceiling for a related frame but
that's not verified for this one), and the SUBCMD-4 finish frame (to confirm MD5-as-hex-string vs
raw digest).

---

## 7. Suggested QGC integration shape

This needs a **new UDP client**, not an extension of `RcuSession` (that class's transport is the
serial `/dev/ttyHS1` link, which is the wrong subsystem for this — see §5). Sketch:

```cpp
// New: src/Siyi/fpv_upgrade_client.h — UDP, port 37250, to the ground unit's IP
enum class FpvUpgradeResult { OK, PACK_REPEAT, ERR_PACK_NUM, ERR_FILE, ERR_CHECK_SUM,
                               ERR_WRITE_FILE_SIZE, ERR_FILE_SIZE,
                               ERR_CONFIG_VERSION_MISMATCH, TIMEOUT };
class FpvUpgradeClient {
public:
    bool open(const std::string& groundIp = "192.168.144.12", uint16_t port = 37250);
    // SUBCMD 2 -> wait ack(fileId) -> SUBCMD 3 loop (chunked, retry on PACK_REPEAT) ->
    // SUBCMD 4 (len+md5-hex) -> wait ack -> SUBCMD 5 (confirm/apply) -> wait ack
    FpvUpgradeResult upgrade(const std::string& binPath,
                             std::function<void(int percent)> progress);
};
```

Frame build/parse (STX/CTRL/VER/LEN/HCRC8/SEQ/TARGET=0xD0/SOURCE=0x1E/CMD_ID=111/SUBCMD/DATA/CRC16
per §2) is new code — it does not reuse `unirc_frame.h`'s `0x5566` framing or `rcu_session.cpp`'s
`AA` framing, though the CRC8/CRC16 table algorithms are likely the same families already in
`crc.h` (worth checking before reimplementing). **Do not implement SUBCMD 3/4/5 byte-exact until
§6 is captured** — only SUBCMD 2's *send* side is currently confirmed; guessing the rest risks
bricking real hardware.
