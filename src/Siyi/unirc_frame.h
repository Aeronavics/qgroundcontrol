#pragma once
//
// SIYI UniRC "0x5566" protocol frame (External SDK, /dev/ttyHS3).
//
//   STX(2)=55 66 | CTRL(1) | DATA_LEN(2 LE) | SEQ(2 LE) | CMD_ID(1) | DATA[..] | CRC16(2 LE)
//
//   CTRL bit0 = need_ack, bit1 = ack_pack. CRC-16/XMODEM over STX..end-of-DATA.
//
// Faithful port of UniRcFrame.java. Header-only (build + streaming Parser + LE
// helpers all inline) so it can be dropped straight into a build.
//
#include <cstdint>
#include <cstddef>
#include <vector>
#include <utility>
#include "crc.h"

namespace unircsdk {

struct UniRcFrame {
    int ctrl = 0;
    int seq = 0;
    int cmdId = 0;
    std::vector<uint8_t> data;

    UniRcFrame() = default;
    UniRcFrame(int ctrl_, int seq_, int cmdId_, std::vector<uint8_t> data_)
        : ctrl(ctrl_), seq(seq_), cmdId(cmdId_), data(std::move(data_)) {}

    bool isAck() const { return (ctrl & 0x02) != 0; }

    // Build a wire frame.
    static std::vector<uint8_t> build(int cmdId, int seq, bool needAck,
                                      const std::vector<uint8_t>& data) {
        size_t dl = data.size();
        std::vector<uint8_t> f(8 + dl + 2);
        size_t p = 0;
        f[p++] = 0x55; f[p++] = 0x66;
        f[p++] = static_cast<uint8_t>(needAck ? 0x01 : 0x00);
        f[p++] = static_cast<uint8_t>(dl & 0xFF);
        f[p++] = static_cast<uint8_t>((dl >> 8) & 0xFF);
        f[p++] = static_cast<uint8_t>(seq & 0xFF);
        f[p++] = static_cast<uint8_t>((seq >> 8) & 0xFF);
        f[p++] = static_cast<uint8_t>(cmdId & 0xFF);
        for (size_t i = 0; i < dl; ++i) f[p++] = data[i];
        uint16_t crc = crc16_xmodem(f.data(), p);
        f[p++] = static_cast<uint8_t>(crc & 0xFF);
        f[p]   = static_cast<uint8_t>((crc >> 8) & 0xFF);
        return f;
    }

    // Streaming framer: feed arbitrary byte chunks (serial) or whole datagrams
    // (UDP); returns any complete, CRC-valid frames found, keeps partial tails.
    class Parser {
    public:
        std::vector<UniRcFrame> feed(const uint8_t* chunk, size_t n) {
            std::vector<UniRcFrame> out;
            acc_.insert(acc_.end(), chunk, chunk + n);
            // Bound growth on sustained garbage (mirrors the Java 8192 cap).
            if (acc_.size() > 8192) {
                acc_.erase(acc_.begin(), acc_.end() - 4096);
            }
            const uint8_t* a = acc_.data();
            size_t len = acc_.size();
            size_t i = 0;
            while (i + 10 <= len) {
                if (a[i] != 0x55 || a[i + 1] != 0x66) { ++i; continue; }
                int dl = a[i + 3] | (a[i + 4] << 8);
                if (dl > 512) { ++i; continue; }
                size_t total = 8 + static_cast<size_t>(dl) + 2;
                if (i + total > len) break;
                uint16_t crcCalc = crc16_xmodem(a + i, 8 + static_cast<size_t>(dl));
                uint16_t crcPkt = static_cast<uint16_t>(
                    a[i + 8 + dl] | (a[i + 8 + dl + 1] << 8));
                if (crcCalc == crcPkt) {
                    int ctrl = a[i + 2];
                    int seq  = a[i + 5] | (a[i + 6] << 8);
                    int cmd  = a[i + 7];
                    std::vector<uint8_t> d(a + i + 8, a + i + 8 + dl);
                    out.emplace_back(ctrl, seq, cmd, std::move(d));
                    i += total;
                } else {
                    ++i; // resync
                }
            }
            if (i > 0) acc_.erase(acc_.begin(), acc_.begin() + i);
            return out;
        }

    private:
        std::vector<uint8_t> acc_;
    };
};

// little-endian helpers (mirror UniRcFrame.u16/s16/u32)
inline uint16_t u16(const uint8_t* d, size_t o) {
    return static_cast<uint16_t>(d[o] | (d[o + 1] << 8));
}
inline int16_t s16(const uint8_t* d, size_t o) {
    return static_cast<int16_t>(u16(d, o));
}
inline uint32_t u32(const uint8_t* d, size_t o) {
    return static_cast<uint32_t>(d[o]) |
           (static_cast<uint32_t>(d[o + 1]) << 8) |
           (static_cast<uint32_t>(d[o + 2]) << 16) |
           (static_cast<uint32_t>(d[o + 3]) << 24);
}

} // namespace unircsdk
