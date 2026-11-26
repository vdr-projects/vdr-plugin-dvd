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

/*****
 *
 * This file is part of the VDR program.
 * This file was part of the OMS program.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****/

/*
 * subpic_decode.c - converts DVD subtitles to an XPM image
 *
 * Mostly based on hard work by:
 *
 * Copyright (C) 2000   Samuel Hocevar <sam@via.ecp.fr>
 *                       and Michel Lespinasse <walken@via.ecp.fr>
 *
 * Lots of rearranging by:
 *      Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *      Thomas Mirlacher <dent@cosy.sbg.ac.at>
 *              implemented reassembling
 *              cleaner implementation of SPU are saving
 *              overlaying (proof of concept for now)
 *              ... and yes, it works now with oms
 *              added tranparency (provided by the SPU hdr)
 *              changed structures for easy porting to MGAs DVD mode
 *
 * addapted for VDR by Andreas Schultz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * added some functions to extract some informations of the SPU command
 * sequence (cSPUassembler::checkSPUHeader and related).
 * needed for 'forced subtitles only' mode
 *
 * weak@pimps.at
 *
 * Code of checkSPUHeader and related functions are based on SPUClass.cpp from the NMM - Network-Integrated Multimedia
 * Environment (www.networkmultimedia.org)
 *
 */

/**
 * fixed and willing to merge with xine's solution ..
 *
 * sven goethel sgoethel@jausoft.com
 */

#include "dvdspu.h"

// C++ Standard Library
#include <cstdint>
#include <cstdlib>
#include <cstring>

// --- simpleFIFO ------------------------------------------------------------

simpleFIFO::simpleFIFO(int Size) {
    size = Size;
    head = tail = 0;
    buffer = static_cast<uint8_t *>(malloc(static_cast<size_t>(size)));
}

simpleFIFO::~simpleFIFO() { free(buffer); }

auto simpleFIFO::Put(const uint8_t *Data, int Count) -> int {
    if (Count > 0) {
        const int rest = size - head;
        const int free = Free();

        if (free <= 0) {
            return 0;
        }
        if (free < Count) {
            Count = free;
        }
        if (Count >= rest) {
            memcpy(buffer + head, Data, static_cast<size_t>(rest));
            if (Count - rest) {
                memcpy(buffer, Data + rest, static_cast<size_t>(Count - rest));
            }
            head = Count - rest;
        } else {
            memcpy(buffer + head, Data, static_cast<size_t>(Count));
            head += Count;
        }
    }
    return Count;
}

auto simpleFIFO::Get(uint8_t *Data, int Count) -> int {
    if (Count > 0) {
        const int rest = size - tail;
        const int cont = Available();

        if (rest <= 0) {
            return 0;
        }
        if (cont < Count) {
            Count = cont;
        }
        if (Count >= rest) {
            memcpy(Data, buffer + tail, static_cast<size_t>(rest));
            if (Count - rest) {
                memcpy(Data + rest, buffer, static_cast<size_t>(Count - rest));
            }
            tail = Count - rest;
        } else {
            memcpy(Data, buffer + tail, static_cast<size_t>(Count));
            tail += Count;
        }
    }
    return Count;
}

// --- cSPUassembler ---------------------------------------------------------

cSPUassembler::cSPUassembler() : simpleFIFO(SPU_BUFFER_SIZE) {
    pts = 0;
    stream = -1;
}

void cSPUassembler::Reset() {
    Clear();
    stream = -1;
}

auto cSPUassembler::Put(const uint8_t *Data, int Count, bool HasPts, uint64_t Pts, int Stream) -> int {
    // The packets of a unit that a seek cut off, or of the stream shown
    // before, don't belong to the next unit: appended to them it came out
    // garbled, and so did every unit after it.
    if (Stream != stream) {
        Reset();
        stream = Stream;
    }
    // The first packet of a unit has its PTS. Not that of the last packet
    // with a PTS, which can be an audio packet's.
    if (HasPts) {
        pts = Pts;
    }
    return simpleFIFO::Put(Data, Count);
}
