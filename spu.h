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

#ifndef __SPU_H__
#define __SPU_H__

#include <inttypes.h>

#define SPU_BUFFER_SIZE         (128*1024)

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

class cSPUassembler: public simpleFIFO
{
 private:
    uint32_t pts;
 public:
    cSPUassembler();

    bool ready(void)
	{ return ((Available() > 2) && (Available() >= getSize())); };
    int getSize(void) { return ((operator[](0)) << 8) | operator[](1); };
    int getPts(void) { return pts; };

    int Put(const uint8_t *Data, int Count, uint32_t Pts);

    int getSPUCommand( unsigned char* packet, unsigned int size );
    
    unsigned int getNextBytes( unsigned int num );
    void setByte( unsigned char* byte );

   /* actual byte position in the encoded stream */
	unsigned char* byte_pos;

   /* size of the spu */
   unsigned int spu_size;
   /* amount of data received */
   unsigned int spu_dataReceived;
   /* offset to the command sequence section */
   unsigned int spu_offset;
   /* command at 1st byte of packet? */
   bool spu_commandOverhead;
   /* command found but another data packet to receive? */
   bool spu_packetOverhead;   
};

#endif
