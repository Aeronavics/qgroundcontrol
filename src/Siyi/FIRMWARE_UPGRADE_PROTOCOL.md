# SIYI UniRC7 Pro — ground unit FPV (image-transmission) firmware upgrade

How UniGCS updates the **ground unit's FPV / image-transmission module** firmware, and how to
replicate it in QGC. Derived from a decompile of UniGCS 3.1.4 **plus a full live capture of a
successful upgrade** on this hardware (2026-08-13), then validated by running our own
implementation against the real unit.

Not covered here: the RC handheld's own MCU, the aircraft RC receiver, and the gimbal/camera —
all different subsystems (§6).

Confidence key: ✅ observed on the wire in a successful upgrade. 🔬 independently verified against
the hardware by us. 🟡 decompile-only, never observed.

---

## 0. Read this first: USB tethering breaks the upgrade

**Plugging USB into the UniRC7 swaps the Android SoC's link away from the radio module to the
external computer.** With USB connected, the internal `eth0` (`192.168.144.20`) simply does not
exist, so nothing can reach the ground unit and every FPV upgrade fails.

```
USB unplugged →  33: eth0  inet 192.168.144.20/24   ← radio/video module link present
USB plugged   →  (no eth0 at all)                    ← upgrade cannot work
```

This is a trap for anyone debugging this: the obvious instinct is to plug in USB and run `adb
logcat`, which silently guarantees the failure you are trying to diagnose. Several early captures
for this document were wasted that way.

**Use wireless debugging instead** (Settings → Developer options → Wireless debugging):

```bash
adb connect <controller-ip>:<port>     # e.g. 192.168.2.105:42981
```

The controller is on both networks at once — `wlan0` on the normal WiFi for adb, `eth0` on the
radio link for the upgrade — so they no longer conflict. Confirm before capturing:

```bash
adb shell ip -4 addr show eth0      # expect 192.168.144.20/24
adb shell ping -c1 192.168.144.12   # expect a reply from the ground unit
```

Separately: **do not run a raw `cat /dev/ttyHS1` capture in parallel.** A tty delivers each byte to
only one reader, so it steals bytes from UniGCS and causes spurious failures. `CAPTURE_PLAYBOOK.md`
calls that capture "read-only-safe"; that is not true for timing-sensitive flows like this one.

---

## 1. The actual upgrade sequence ✅

Captured end to end. **The firmware bytes travel over FTP, not over the UDP channel** — the UDP
channel is control only:

| # | Step | Transport | Observed |
|---|---|---|---|
| 1 | Announce filename | UDP `SUBCMD 2` | `13:48:49.931` → reply `13:48:49.936` |
| 2 | **Upload the firmware** | **FTP `STOR`** | `13:48:49.972` "Current FTP Dir: /" → `13:49:00.296` "uploadFile complete" (~10 s) |
| 3 | Flash + reboot | UDP `SUBCMD 5` | `13:49:04.252` → reply `13:49:04.257` |
| 4 | Wait for it to come back | UDP `SUBCMD 6`, polled ~2 s | 34 polls; first answer `13:50:41.372` (**~97 s** after step 3), then a **second reboot** of ~65 s |

> An earlier revision of this document predicted a UDP-chunked transfer via `SUBCMD 3`/`4`, because
> that is what the decompiled code suggested. **The capture disproved it — zero `SUBCMD 3`/`4`
> frames were sent.** Those belong to the *other* file-transfer mode (`h4.a.UDP`); this hardware is
> configured `h4.a.FTP`. The enum named `FTP` was literal, not a label — an earlier note in this
> file explicitly dismissed it as "no TCP, no FTP", which was wrong.

---

## 2. UDP control channel ✅ 🔬

**`192.168.144.12:37250`**, plain `DatagramSocket`. Frame layout — every field verified byte-exact
against captured frames, and reproduced byte-for-byte by our builder (`FpvUpgradeClient::selfTest()`
checks three real frames):

| off | field | size | notes |
|---|---|---|---|
| 0 | STX | 1 | `0xAA` |
| 1 | CTRL | 1 | bit0 need_ack, bit1 ack_pack, bits2-3 crcType (`2` = CRC16) → observed `0x09` |
| 2 | VER | 1 | `0x03` (V3: 2-byte length, CMD_ID present) |
| 3-4 | DATA_LEN | 2 LE | length of DATA only |
| 5 | HCRC8 | 1 | `crc8_maxim(frame[0..4])` |
| 6-7 | SEQ | 2 LE | increments per frame |
| 8 | **SRC** | 1 | sender — `0xD0` `ParamAdTool`(208) = us |
| 9 | **DST** | 1 | receiver — `0x1E` `Transmission`(30) = FPV module |
| 10 | CMD_ID | 1 | `0x6F` (111) |
| 11 | SUBCMD | 1 | below |
| 12.. | DATA | n | |
| last 2 | CRC16 | 2 LE | `crc16_xmodem` over everything except these two bytes |

**SRC/DST swap by direction** — outbound `D0 1E`, replies `1E D0`. (Cross-checked on the RCU-serial
link: `D0 10` out, `10 D0` in.) Match replies on the swapped form.

**Both CRCs are exactly the ones already in `crc.h`** — no new CRC code was needed:
`crc8_maxim(frame[0..4])` → `0x4E` ✓, `crc16_xmodem(frame[0..end-2])` → `0x0916` stored LE `16 09` ✓.

### SUBCMDs

| SUBCMD | Dir | Payload | Reply | State |
|---|---|---|---|---|
| `2` START | → | firmware filename (ASCII) | `[01, u16 LE]` — observed `01 00 7D` | ✅ both directions |
| `3` CHUNK | → | — | — | 🟡 other transfer mode; **never sent** |
| `4` FINISH | → | — | — | 🟡 other transfer mode; **never sent** |
| `5` CONFIRM | → | *(empty)* | `[01]` = OK; flashes + reboots | ✅ both directions |
| `6` VERSION | → | *(empty)* | ASCII version, e.g. `CX6653C-N_A5.1.2.6` | ✅ 🔬 |

Result codes (`m5.d`) — note the decompiled mapper treats **any unlisted value as OK**, which our
port reproduces rather than "fixes":
`OK=1, PACK_REPEAT=3, ERR_PACK_NUM=-1, ERR_FILE=-3, ERR_CHECK_SUM=-7, ERR_WRITE_FILE_SIZE=-10,
ERR_FILE_SIZE=-13, ERR_CONFIG_VERSION_MISMATCH=-15`.

### Notes on the START exchange

* **The filename really does end in `.zip1`.** SIYI ships the firmware as e.g.
  `CX6653C-N_A5.1.2.6-svn75-2026-06-08.zip1`. An earlier revision of this document flagged the
  trailing `0x31` as a possible protocol field or buffer artefact — it is neither, just the
  extension. Send the basename verbatim; no special handling.
* **The START reply's u16 is a constant, not a file id.** Observed `01 00 7D` → ok=1,
  u16 = `0x7D00` = **32000**, and our own client received the identical 32000 in a separate
  session with a different SEQ. So it is a fixed capability/buffer value, *not* per-file state —
  despite the decompile implying a file id. Our API surfaces it raw rather than naming it.
* **Filename prefix gating.** UniGCS matches the name's prefix (before the first `_`) against the
  unit's hardware id — here `CX6653C-N`, which is also the prefix of the version string the unit
  reports. The vendor bundle ships a second file (`7-VTS-H_…`) for a different variant; picking the
  wrong one is what that check exists to catch.

---

## 3. FTP bulk transfer ✅ 🔬

Where the firmware actually goes. Server probed directly on the ground unit:

```
$ nc 192.168.144.12 209
220 Operation successful
USER anonymous → 230 Operation successful      (logs in immediately, any credentials)
PASS x         → 230 Operation successful
SYST           → 215 UNIX Type: L8
PWD            → 257 "/"
FEAT           → EPSV, PASV, REST STREAM, MDTM, SIZE
```

* **Host** `192.168.144.12` (ground) — the same address the UDP channel uses.
* **Port `209`** — the upgrade path's own config constant. **Port `200` also runs the same server**
  (that is what an in-app debug screen uses); port 21 is refused. Either works.
* **Anonymous**, binary mode, **passive**, upload into `/`.
* The module **consumes the file after flashing** — it is gone from `/` afterwards, so do not try
  to verify by listing.

---

## 4. QGC implementation

| File | What |
|---|---|
| `udp_port.h` / `.cpp` | POSIX UDP socket (unconnected `sendto`/`recvfrom`, `SO_RCVTIMEO`), std+POSIX only like `serial_port.*` so it builds on Android |
| `fpv_frame.h` | The `0xAA` V3 framing, header-only, reuses `crc.h`. A **third** framing alongside `unirc_frame.h` (0x5566) and `rcu_session`'s `AA` |
| `ftp_upload.h` / `.cpp` | Minimal FTP `STOR` (anonymous, binary, PASV) — dependency-free, no libcurl/Qt |
| `fpv_upgrade_client.h` / `.cpp` | The client: `queryVersion()`, `sendStart()`, `sendConfirm()`, `waitForReboot()`, `upgrade()` |

Built as the `Siyi` static library (`src/Siyi/CMakeLists.txt`), linked into QGC when
`QGC_SIYI_ENABLED` is on — the default on every platform except Windows, where the POSIX
transports cannot build.

```cpp
FpvUpgradeClient c;
c.open();                                   // 192.168.144.12:37250
std::string ver;
c.queryVersion(ver);                        // liveness + current version
std::string newVer;
c.upgrade("/path/CX6653C-N_A5.1.2.6-….zip", // START → FTP STOR → CONFIRM → wait
          [](int pct){ /* 0-100 */ }, &newVer);
```

### Verification status

* **`selfTest()`** rebuilds three real captured frames byte-for-byte (START with a 40-byte payload,
  CONFIRM and VERSION with empty payloads and non-zero SEQ), round-trips a parse, and asserts a
  corrupted frame is rejected. It pins the framing, both CRCs and the SRC/DST order at once.
* 🔬 **A complete upgrade has been performed by this client on real hardware.** Cross-compiled
  for the controller's arm64 Android and run on-device against the ground unit, it executed the
  whole sequence — `START` → 111 MB FTP `STOR` → size verify → `CONFIRM` → reboot — and the module
  came back reporting `CX6653C-N_A5.1.2.6`. Ping, UDP and FTP were all healthy afterwards.
* **Use `ftpFileSize()` as an integrity gate.** After uploading, confirm the server reports exactly
  the local byte count *before* sending `CONFIRM`. A truncated upload followed by a commit is how
  you brick the module. Our test harness refuses to commit unless the sizes match.
* **`CONFIRM` does not stop the unit answering — do not treat the next reply as "done".**
  Measured on hardware: `CONFIRM` was acknowledged, the unit kept answering `SUBCMD 6` from the
  **old** firmware, and only dropped off ~1 minute later, coming back ~2 minutes after that. A
  naive "poll until it replies" therefore reports success almost instantly, while the module is
  still writing itself — which is exactly the moment a user might power off or fly. Our first
  implementation had this bug.
* **Wait for it to go DOWN, then come back, then STAY back.** Expect two reboots. `waitForReboot()`
  now requires the unit to disappear first, then answer continuously for `REBOOT_STABLE_MS` (90 s),
  restarting that window if it drops again. Budget several minutes overall.
* **A version string alone does not prove the flash took**, especially when reflashing the same
  version — the reply may be coming from the firmware you are replacing.
* `sendChunk()`/`sendFinish()` return `NotImplemented` by design: they belong to the unused
  transfer mode, were never observed, and would write flash from guessed byte layouts.

---

## 5. Reproducing the capture

```bash
# USB MUST be unplugged — see §0
adb connect <controller-ip>:<port>
adb shell ip -4 addr show eth0        # must show 192.168.144.20
adb logcat -c && adb logcat -v time > cap.txt &
# UniGCS: Device Info → GROUND_IMAGE → pick firmware → Upgrade
```

Decode with: `WriteTask` = outbound frames (hex, decode per §2), `SIYITransParser` = UDP replies,
`FtpClient` = the FTP transfer.

---

## 6. Adjacent subsystems (not this document)

| Target | Transport | Notes |
|---|---|---|
| RC handheld MCU (`GROUND_MCU`) | `/dev/ttyHS1` serial, group `240`, cmd `32`/`35`/`38` | destination hardcoded `q.RCU`(16) |
| Aircraft RC receiver (`SKY_MCU`) | same serial path | destination hardcoded `q.Receiver`(19) |
| Air-unit FPV (`SKY_IMAGE`) | same protocol against `192.168.144.11` | investigated — see §7 |
| Gimbal / camera | UDP `:37280` | separate subsystem |

The serial path **cannot** address the FPV module: those commands hardcode `q.RCU`/`q.Receiver` and
there are no references to `Transmission`(30) anywhere under `l5/` or `biz/siyi/core/rcu/`. So the
FPV upgrade genuinely requires the network path, with no serial fallback.


---

## 7. Air unit (`SKY_IMAGE`) — investigation 🔬

Probed with an air unit connected and bound, and now **implemented** (see "Implementation" below).

### It is the same protocol, and the same firmware file

`.11` is a genuinely separate device (distinct MAC `00:01:66:…` vs the ground unit's `00:01:4c:…`)
and speaks the identical stack: UDP `:37250` CMD_ID 111, plus anonymous FTP on **209 and 200**.

Both units report hardware id **`CX6653C-N`**, and a non-destructive `SUBCMD 2` probe (announce
only — nothing is written until `CONFIRM`) settles which file each accepts:

| announced name | air `.11` | ground `.12` |
|---|---|---|
| `CX6653C-N_A5.1.2.6-…zip1` | **OK** (ack 32000) | **OK** (ack 32000) |
| `7-VTS-H_A5.1.2.6-…zip1` | `ERR_FILE` | `ERR_FILE` |
| deliberately bogus name | `ERR_FILE` | `ERR_FILE` |

⇒ **The air unit takes the SAME `CX6653C-N` firmware as the ground unit.** The vendor bundle's
second file (`7-VTS-H_…`, near-identical size) is for a different hardware variant and is rejected
by both — do not offer it for this kit. The bogus-name row also shows the prefix gate is a real
safety net, not advisory.

### RF throughput, and what the prelude is actually for ✅

Measured with an 8 MB FTP upload (temp file, deleted afterwards; `DELE` → 250 on both):

| condition | throughput |
|---|---|
| ground `.12` | **10.2 MB/s** |
| air `.11`, upgrade mode **OFF** | **0.12 MB/s** |
| air `.11`, upgrade mode **OFF** (repeat, later session) | **0.11 MB/s** — reproducible |
| air `.11`, upgrade mode **ON** (real update) | **substantially faster** (not yet quantified) |

⇒ **This is what `SUBCMD 116` is for.** The slow figure is reproducible whenever the prelude has
*not* been run, and a real update with the prelude enabled completed far quicker. So `0x74` lifts
usable RF bandwidth for the transfer — almost certainly by quieting the video/telemetry stream.
Skipping it "works" but is roughly two orders of magnitude slower.

**Do not quote ~15 minutes** (an earlier revision of this document did): that extrapolation came
from the un-prepared link and does not describe the real path. The with-prelude rate has not been
measured — the log buffer had rolled over before it could be captured.

Consequences for any implementation:
* `ftpStoreFile()`'s default 10 s timeout is **too short** for the air unit — it must be raised
  substantially, and the socket timeouts are per-operation, not per-transfer.
* A long RF transfer must survive link interruptions; there is currently no resume (the server
  does advertise `REST STREAM`, so resume is at least possible).
* The UI must warn against moving or powering down the aircraft for the duration.

### The sky path has a prelude the ground path does not 🟡

From the decompile (`j1.java`), `SKY_IMAGE` — unlike `GROUND_IMAGE` — first does:

1. `s.f0(true)` → **RCU-serial** (`ttyHS1`, not UDP) `CMD_ID 20`, `SUBCMD 116`, payload `[1]`
2. wait **12 s**
3. poll `CMD_ID 20`, `SUBCMD 115` (no payload), up to 4× at 1 s — reply byte `0` means ready
   (`l5/j.java` tests `(data[0] & 255) == 0`)
4. only then upload

**What 116 does is now evidenced** (see the throughput table above): it is *not* IP routing — `.11`
is reachable by ping, UDP and FTP with no prelude — but it makes the transfer dramatically faster,
consistent with it quieting the video/telemetry stream. Replicate the vendor sequence.

**Implemented** as `RcuSession::setSkyUpgradeMode()` / `getSkyUpgradeReady()` (`TARGET_IMAGE` +
cmd `0x74`/`0x73`). Both frames are verified byte-for-byte against the captured UniGCS bytes.

⚠ One subtlety worth keeping: UniGCS sends the **`0x73` query with CTRL `0x0B`**, not the `0x09`
every other command here uses. `RcuSession` gained a `ctrl` override (defaulting to `0x09`, so no
existing command changes) purely so this one reproduces exactly — an avoidable difference is not
worth risking on a path that takes 15 minutes to retest.

### Risk

Flashing the air unit is materially riskier than the ground unit: it is on the aircraft, reachable
only over the very link being updated, and the write follows a 15 minute transfer. A failure part
way through leaves an unbootable module that cannot be recovered over the air. Treat as a
bench operation with the aircraft powered from a stable supply — not a field action.

### Implementation

| Piece | Where |
|---|---|
| Serial prelude (`0x74` set / `0x73` poll, reply `0` == ready) | `RcuSession::setSkyUpgradeMode()` / `getSkyUpgradeReady()` |
| Slow-link FTP timeout (120 s vs 30 s per operation) | `FpvUpgradeClient::FTP_TIMEOUT_SLOW_MS`, passed by `upgrade()` |
| Target choice, prelude sequencing, teardown | `ControllerHandler::upgradeFpvFirmware(url, name, airUnit)` |
| Ground/Air selector + air-specific warnings | `FirmwareUpdate.qml` |

Sequence for the air unit: enable upgrade mode → 12 s settle → poll ready (4× @1 s) → announce →
FTP upload (~15 min) → size verify → `CONFIRM` → wait out both reboots → **disable upgrade mode
again on every exit path**, success or failure.

The UI only offers the air unit when one is actually detected (`airUnitPresent`), and states the
~15 minute duration and the no-over-the-air-recovery risk before you commit.
