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

#define SPU_BUFFER_SIZE         (128*1024)

#define CMD_SPU_FORCE_DISPLAY   0x00
#define CMD_SPU_SHOW            0x01
#define CMD_SPU_HIDE            0x02
#define CMD_SPU_SET_PALETTE     0x03
#define CMD_SPU_SET_ALPHA       0x04
#define CMD_SPU_SET_SIZE        0x05
#define CMD_SPU_SET_PXD_OFFSET  0x06
#define CMD_SPU_WIPE            0x07  /* Not currently implemented */
#define CMD_SPU_ERR             0xfe  /* own: our error tag */
#define CMD_SPU_EOF             0xff


class simpleFIFO {
 private:
    uint8_t *buffer;
    int head;
    int tail;
    int size;

 public:
    simpleFIFO(int Size);
    ~simpleFIFO();
    int Put(const uint8_t *Data, int Count);
    int Get(uint8_t *Data, int Count);
    int Release(int Count);

    int Size(void) const
	{ return size; };
    int Free(void) const
	{ return ((size + tail) - head - 1) % size; };
    int Available(void) const
	{ return ((head + size) - tail) % size; };
    uint8_t const &operator[] (int i) const
	{ return buffer[(tail + i) % size]; };
};

class cSPUCommand
{
  public:
    cSPUCommand(simpleFIFO &fifo);
    void reset(void);

    int initSPUCommand( void );
    int getNextSPUCommand( void );

    int eatenBytes() { return byte_pos; }
    int availableBytes() { return spu_size - byte_pos; }

    uint32_t getNextBytes( int num );
    bool skipBytes(int num);
    bool next_cs_off(void);

    /* actual byte position in the encoded stream */
    int byte_pos;

    /* size of the spu */
    int spu_size;

    uint32_t start_time;

    uint32_t command;

    int next_cs_offset;
    int prev_cs_offset;
    int eof_cs;

    simpleFIFO &data;
};

class cSPUassembler: public simpleFIFO
{
 private:
    uint64_t pts;
 public:
    cSPUassembler();

    bool ready(void)
	{ return ((Available() > 2) && (Available() >= getSize())); };
    int getSize(void) { return ((operator[](0)) << 8) | operator[](1); };
    uint64_t getPts(void) { return pts; };

    int Put(const uint8_t *Data, int Count, uint64_t Pts);

    cSPUCommand spucmd;
};

#endif

