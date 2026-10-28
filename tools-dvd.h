/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#ifndef __TOOLS_DVD_H
#define __TOOLS_DVD_H

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include "mpegtypes.h"

class cPStream {
 public:
    static bool packetStart(const uint8_t *data) {
        return (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01);
    }

    static int packetType     (const uint8_t *data) { return data[3]; }
    static int stuffingLength (const uint8_t *data) { return data[13] & 0x07; }
    static int packetLength   (const uint8_t *data) { return (data[4] << 8) + data[5] + 6; }
    static int PESHeaderLength(const uint8_t *data) { return (data[8]); }

    static uint64_t fromPTS(const uint8_t *p)
    {
                uint64_t lpts = 0;
                lpts  = (((uint64_t)p[0]) & 0x0e) << 29;
                lpts |= ( (uint64_t)p[1])         << 22;
                lpts |= (((uint64_t)p[2]) & 0xfe) << 14;
                lpts |= ( (uint64_t)p[3])         <<  7;
                lpts |= (((uint64_t)p[4]) & 0xfe) >>  1;
                return lpts;

    }

    static void toPTS(uint8_t *p, uint64_t lpts)
    {
            p[0] =       ((uint8_t)(lpts >> 29) & 0x0e) | 0x21;
            p[1] =        (uint8_t)(lpts >> 22);
            p[2] = 0x01 | (uint8_t)(lpts >> 14);
            p[3] =        (uint8_t)(lpts >> 7);
            p[4] = 0x01 | (uint8_t)(lpts << 1);
    }
};

#endif // __TOOLS_DVD_H
