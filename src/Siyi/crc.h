#pragma once
//
// CRC helpers for the SIYI UniRC 7 Pro protocols.
//
//   crc16_xmodem  — CRC-16/XMODEM (poly 0x1021, init 0x0000, no reflection, no
//                   final xor). Used by BOTH the 0x5566 External SDK (ttyHS3) and
//                   the AA internal RCU protocol (ttyHS1) as the whole-frame CRC.
//   crc8_maxim    — CRC-8/MAXIM (Dallas/1-Wire: poly 0x31 reflected = 0x8C, init
//                   0x00, refin/refout, xorout 0x00). Used as the AA-frame header
//                   checksum over the 5 header bytes `AA 09 03 LEN_lo LEN_hi`.
//
// Faithful bitwise ports of Crc16.java / Crc8.java. Header-only + inline so the
// SDK stays dependency-free (std only).
//
#include <cstdint>
#include <cstddef>

namespace unircsdk {

// CRC-16/XMODEM over d[0 .. n-1]. Matches Crc16.calc() (e.g. 0x40 -> 0x9C81).
inline uint16_t crc16_xmodem(const uint8_t* d, size_t n) {
    uint16_t crc = 0;
    for (size_t i = 0; i < n; ++i) {
        crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(d[i]) << 8));
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000)
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
            else
                crc = static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

// CRC-8/MAXIM over d[0 .. n-1]. Matches Crc8.maxim().
inline uint8_t crc8_maxim(const uint8_t* d, size_t n) {
    uint8_t crc = 0;
    for (size_t i = 0; i < n; ++i) {
        crc = static_cast<uint8_t>(crc ^ d[i]);
        for (int b = 0; b < 8; ++b) {
            if (crc & 1)
                crc = static_cast<uint8_t>((crc >> 1) ^ 0x8C);
            else
                crc = static_cast<uint8_t>(crc >> 1);
        }
    }
    return crc;
}

} // namespace unircsdk
