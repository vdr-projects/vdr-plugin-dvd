/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * parts of this file are derived from the OMS program.
 *
 */

#ifndef VDR_DVD_DVDSPU_H
#define VDR_DVD_DVDSPU_H

// C++ Standard Library
#include <cstddef>
#include <cstdint>
#include <vector>

inline constexpr int SPU_BUFFER_SIZE = 128 * 1024;

class simpleFIFO {
  private:
    std::vector<uint8_t> buffer;
    int head{0};
    int tail{0};
    int size;

  public:
    explicit simpleFIFO(int Size) : buffer(static_cast<size_t>(Size)), size(Size) {}
    auto Put(const uint8_t *Data, int Count) -> int;
    auto Get(uint8_t *Data, int Count) -> int;
    void Clear() { head = tail = 0; }

    [[nodiscard]] auto Free() const -> int { return ((size + tail) - head - 1) % size; }
    [[nodiscard]] auto Available() const -> int { return ((head + size) - tail) % size; }
    // The byte Offset places after the oldest.
    [[nodiscard]] auto Peek(int Offset) const -> uint8_t {
        return buffer.at(static_cast<size_t>((tail + Offset) % size));
    }
};

class cSPUassembler : public simpleFIFO {
  private:
    uint64_t pts{0};
    int stream{-1};

  public:
    cSPUassembler();

    // A unit starts with its size and the offset of its control sequence,
    // which ends with at least its delay, the next one's offset and the end
    // command.
    [[nodiscard]] auto ready() const -> bool { return Available() >= 4 && (Available() >= getSize() || !valid()); }
    [[nodiscard]] auto valid() const -> bool {
        return getSize() >= 4 && getCmdOffs() >= 4 && getCmdOffs() + 5 <= getSize();
    }
    [[nodiscard]] auto getSize() const -> int { return (Peek(0) << 8) | Peek(1); }
    [[nodiscard]] auto getCmdOffs() const -> int { return (Peek(2) << 8) | Peek(3); }
    [[nodiscard]] auto getPts() const -> uint64_t { return pts; }

    void Reset();
    // A packet of Stream that isn't put: the unit it belongs to stays incomplete.
    void Skip(int Stream) {
        if (Stream == stream) {
            Reset();
        }
    }
    auto Put(const uint8_t *Data, int Count, bool HasPts, uint64_t Pts, int Stream) -> int;
};

#endif // VDR_DVD_DVDSPU_H
