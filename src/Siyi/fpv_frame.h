#pragma once
//
// SIYI "0xAA V3" protocol frame — the framing used by the FPV / image-transmission
// module's upgrade channel (UDP :37250). This is a THIRD framing, distinct from
// both of the ones already in this SDK:
//
//   unirc_frame.h  0x5566  External SDK          (ttyHS3 @115200)
//   rcu_session.*  0xAA V0-ish, 3-byte header    (ttyHS1 @230400)
//   THIS FILE      0xAA V3, CMD_ID + SUBCMD      (UDP :37250)
//
// Layout (every field verified byte-exact against a live capture, see
// FIRMWARE_UPGRADE_PROTOCOL.md §2):
//
//   off  field     size  notes
//   0    STX       1     0xAA
//   1    CTRL      1     bit0 need_ack, bit1 ack_pack, bits2-3 crcType (2 = CRC16)
//   2    VER       1     0x03 (V3: 2-byte length, CMD_ID present)
//   3-4  DATA_LEN  2 LE  length of DATA only
//   5    HCRC8     1     CRC-8/MAXIM over bytes [0..4]
//   6-7  SEQ       2 LE  auto-increment, wraps at 65536
//   8    SRC       1     sender   (us = 0xD0 ParamAdTool)
//   9    DST       1     receiver (FPV module = 0x1E Transmission)
//   10   CMD_ID    1     0x6F (111) for the upgrade channel
//   11   SUBCMD    1     see FpvUpgradeClient
//   12.. DATA      n
//   last CRC16     2 LE  CRC-16/XMODEM over everything except these 2 bytes
//
// SRC/DST swap between directions — confirmed from captures: app->device frames
// begin `D0 1E`, device->app replies begin `1E D0`. (The equivalent RCU-serial
// frames likewise go `D0 10` outbound / `10 D0` inbound.) Do not assume a fixed
// pair; match replies on the swapped form.
//
// Header-only, std-only, to match unirc_frame.h.
//
#include <cstdint>
#include <cstddef>
#include <vector>
#include <utility>
#include "crc.h"

namespace unircsdk {

struct FpvFrame {
    // ---- wire constants ----
    static constexpr uint8_t STX     = 0xAA;
    static constexpr uint8_t VER_V3  = 0x03;
    static constexpr uint8_t CRC_16  = 0x02;   // CTRL bits2-3 selector

    // Device ids (biz.siyi.protocol.bu.manufacturer.siyi.q)
    static constexpr uint8_t DEV_PARAM_AD_TOOL = 0xD0;  // 208 — the GCS/tool (us)
    static constexpr uint8_t DEV_TRANSMISSION  = 0x1E;  //  30 — FPV image-transmission module
    static constexpr uint8_t DEV_RCU           = 0x10;  //  16 — RC handheld MCU (NOT this channel)
    static constexpr uint8_t DEV_RECEIVER      = 0x13;  //  19 — aircraft RC receiver (NOT this channel)

    static constexpr uint8_t CMD_ID_UPGRADE = 0x6F;     // 111

    // Fixed overhead: 12 header bytes + 2 CRC bytes.
    static constexpr size_t HEADER_LEN = 12;
    static constexpr size_t OVERHEAD   = HEADER_LEN + 2;

    uint8_t  src    = 0;
    uint8_t  dst    = 0;
    uint8_t  cmdId  = 0;
    uint8_t  subCmd = 0;
    uint16_t seq    = 0;
    bool     needAck = false;
    std::vector<uint8_t> data;

    // Build a wire frame. `data` may be empty.
    static std::vector<uint8_t> build(uint8_t src, uint8_t dst,
                                      uint8_t cmdId, uint8_t subCmd,
                                      uint16_t seq,
                                      const std::vector<uint8_t>& data,
                                      bool needAck = true) {
        const size_t dl = data.size();
        std::vector<uint8_t> f(OVERHEAD + dl);

        f[0] = STX;
        f[1] = static_cast<uint8_t>((needAck ? 0x01 : 0x00) | (CRC_16 << 2));
        f[2] = VER_V3;
        f[3] = static_cast<uint8_t>(dl & 0xFF);
        f[4] = static_cast<uint8_t>((dl >> 8) & 0xFF);
        f[5] = crc8_maxim(f.data(), 5);              // header checksum over [0..4]
        f[6] = static_cast<uint8_t>(seq & 0xFF);
        f[7] = static_cast<uint8_t>((seq >> 8) & 0xFF);
        f[8] = src;
        f[9] = dst;
        f[10] = cmdId;
        f[11] = subCmd;
        for (size_t i = 0; i < dl; ++i) f[HEADER_LEN + i] = data[i];

        const size_t crcOver = HEADER_LEN + dl;
        uint16_t crc = crc16_xmodem(f.data(), crcOver);
        f[crcOver]     = static_cast<uint8_t>(crc & 0xFF);
        f[crcOver + 1] = static_cast<uint8_t>((crc >> 8) & 0xFF);
        return f;
    }

    // Parse one datagram. Returns false unless the frame is complete and both
    // checksums validate. UDP is one-frame-per-datagram so no streaming framer
    // is needed here (unlike the serial links).
    static bool parse(const uint8_t* d, size_t n, FpvFrame& out) {
        if (!d || n < OVERHEAD) return false;
        if (d[0] != STX || d[2] != VER_V3) return false;
        if (crc8_maxim(d, 5) != d[5]) return false;

        const size_t dl = static_cast<size_t>(d[3]) | (static_cast<size_t>(d[4]) << 8);
        if (OVERHEAD + dl > n) return false;

        const size_t crcOver = HEADER_LEN + dl;
        const uint16_t crcCalc = crc16_xmodem(d, crcOver);
        const uint16_t crcPkt  = static_cast<uint16_t>(d[crcOver] | (d[crcOver + 1] << 8));
        if (crcCalc != crcPkt) return false;

        out.needAck = (d[1] & 0x01) != 0;
        out.seq     = static_cast<uint16_t>(d[6] | (d[7] << 8));
        out.src     = d[8];
        out.dst     = d[9];
        out.cmdId   = d[10];
        out.subCmd  = d[11];
        out.data.assign(d + HEADER_LEN, d + HEADER_LEN + dl);
        return true;
    }
};

} // namespace unircsdk
