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

//#define DEBUG

/*
 * subpic_decode.c - converts DVD subtitles to an XPM image
 *
 * Mostly based on hard work by:
 *
 * Copyright (C) 2000   Samuel Hocevar <sam@via.ecp.fr>
 *                       and Michel Lespinasse <walken@via.ecp.fr>
 *
 * Lots of rearranging by:
 *	Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *	Thomas Mirlacher <dent@cosy.sbg.ac.at>
 *		implemented reassembling
 *		cleaner implementation of SPU are saving
 *		overlaying (proof of concept for now)
 *		... and yes, it works now with oms
 *		added tranparency (provided by the SPU hdr)
 *		changed structures for easy porting to MGAs DVD mode
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
 * Code of checkSPUHeader and related functions are based on SPUClass.cpp from the NMM - Network-Integrated Multimedia Environment (www.networkmultimedia.org)
 *
 */

#include <assert.h>

#include <inttypes.h>
#include <malloc.h>
#include <string.h>

#include "spu.h"


//----------- cSPUdata -----------------------------------

simpleFIFO::simpleFIFO(int Size)
{
    size = Size;
    head = tail = 0;
    buffer = (uint8_t *)malloc(2*size);
}

simpleFIFO::~simpleFIFO()
{
    free(buffer);
}

int simpleFIFO::Put(const uint8_t *Data, int Count)
{
  if (Count > 0) {
     int rest = size - head;
     int free = Free();

     if (free <= 0)
        return 0;
     if (free < Count)
        Count = free;
     if (Count >= rest) {
        memcpy(buffer + head, Data, rest);
        if (Count - rest)
           memcpy(buffer, Data + rest, Count - rest);
        head = Count - rest;
        }
     else {
        memcpy(buffer + head, Data, Count);
        head += Count;
        }
     }
  return Count;
}

int simpleFIFO::Get(uint8_t *Data, int Count)
{
  if (Count > 0) {
     int rest = size - tail;
     int cont = Available();

     if (rest <= 0)
        return 0;
     if (cont < Count)
        Count = cont;
     if (Count >= rest) {
	 memcpy(Data, buffer + tail, rest);
	 if (Count - rest)
	     memcpy(Data + rest, buffer, Count - rest);
	 tail = Count - rest;
     } else {
	 memcpy(Data, buffer + tail, Count);
	 tail += Count;
     }
  }
  return Count;
}

int simpleFIFO::Release(int Count)
{
  if (Count > 0) {
     int rest = size - tail;
     int cont = Available();

     if (rest <= 0)
        return 0;
     if (cont < Count)
        Count = cont;
     if (Count >= rest)
	tail = Count - rest;
     else
	tail += Count;
  }
  return Count;
}

// ---------- cSPUassembler -----------------------------------

cSPUassembler::cSPUassembler(): simpleFIFO(SPU_BUFFER_SIZE) { };

int cSPUassembler::Put(const uint8_t *Data, int Count, uint32_t Pts)
{
    if (Pts > 0)
	pts = Pts;
    return simpleFIFO::Put(Data, Count);
}

unsigned int cSPUassembler::getNextBytes( unsigned int num ) {
   unsigned int result = 0;

   if( num>4 ) {
     //cout << "getBytes used with more than 4 bytes" << endl;
     return( result );
   }

   while( num > 0 ) {
     result <<= 8;
     result |= *byte_pos;
     //byte_pos++;
     byte_pos++;
     num--;
   }

   return( result );
}

void cSPUassembler::setByte( unsigned char* byte ) {
   byte_pos = byte;
}

int cSPUassembler::getSPUCommand( unsigned char* packet, unsigned int size ) {
   unsigned char command;
   unsigned int  next_cs_offset;

   // set data pointer to begin of the packet
   setByte( packet );

   //check if dealing with spanning data
   if( spuOffsetLast > 0 ) {
  		spu_size = size;

		//check for spanning packet
		if ( spu_size < spuOffsetLast ) return ( spuOffsetLast - spu_size + 1 );

    	// set data pointer to begin of the control sequence
    	setByte( packet + spuOffsetLast );
     	getNextBytes(2);
     	spuOffsetLast = 0;

	}
	//no spanning packet
	else {

   		// first 2 bytes contains the spu size
	   	spu_size = getNextBytes(2);

	   	// get offset to the Control Sequence
   		next_cs_offset = getNextBytes(2);

   	   	//cout << "SPU Size: " << spu_size << endl;
   		if ( next_cs_offset == 0 ) return 5;
   		if ( next_cs_offset > size ) return (next_cs_offset - size);

   		// set data pointer to begin of the control sequence
   		setByte( packet+next_cs_offset );

   		// first 2 bytes of the control sequence contains the start time
   		getNextBytes(2);
   		//cout << "Start Time: " << start_time << endl;
   		next_cs_offset = getNextBytes(2);
   	}

   	while( (command = getNextBytes(1)) != 0xff ) {
     //cout << "Display Control Command: " << (int) command;

     switch( command ) {
       case 0x00: // forced play
         //cout << "  Menu" << endl;
         return 0;
       break;

       case 0x01: // display start
         //cout << "  display start" << endl;
         return 1;
       break;

       case 0x02: // display stop
         //cout << "  display stop" << endl;
         return 2;
       break;
	 }//switch
   	} //while
   	return 4;
}
