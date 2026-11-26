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

#ifndef __DVDSPU_H__
#define __DVDSPU_H__

#include <inttypes.h>

#define SPU_BUFFER_SIZE (128 * 1024)

class simpleFIFO {
private:
    uint8_t *buffer;
    int head;
    int tail;
    int size;

public:
    explicit simpleFIFO(int Size);
    simpleFIFO(const simpleFIFO &) = delete;
    simpleFIFO &operator=(const simpleFIFO &) = delete;
    ~simpleFIFO();
    int Put(const uint8_t *Data, int Count);
    int Get(uint8_t *Data, int Count);
    void Clear(void) { head = tail = 0; }

    int Free(void) const { return ((size + tail) - head - 1) % size; };
    int Available(void) const { return ((head + size) - tail) % size; };
    uint8_t const &operator[](int i) const { return buffer[(tail + i) % size]; };
};

class cSPUassembler : public simpleFIFO {
private:
    uint64_t pts;
    int stream;

public:
    cSPUassembler();

    // A unit starts with its size and the offset of its control sequence,
    // which ends with at least its delay, the next one's offset and the end
    // command.
    bool ready(void) const { return Available() >= 4 && (Available() >= getSize() || !valid()); };
    bool valid(void) const { return getSize() >= 4 && getCmdOffs() >= 4 && getCmdOffs() + 5 <= getSize(); }
    int getSize(void) const { return ((operator[](0)) << 8) | operator[](1); };
    int getCmdOffs(void) const { return ((operator[](2)) << 8) | operator[](3); };
    uint64_t getPts(void) const { return pts; };

    void Reset(void);
    // A packet of Stream that isn't put: the unit it belongs to stays incomplete.
    void Skip(int Stream)
    {
        if (Stream == stream)
            Reset();
    }
    int Put(const uint8_t *Data, int Count, bool HasPts, uint64_t Pts, int Stream);
};

#endif // __DVDSPU_H__
