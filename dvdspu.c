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

#if 0

#define CTRLDEBUG
#undef DEBUG_CONTROL
#define DEBUG_CONTROL(format, args...) printf (format, ## args); fflush(NULL)

#else

#ifdef CTRLDEBUG
#undef CTRLDEBUG
#endif
#ifndef DEBUG_CONTROL
#define DEBUG_CONTROL(format, args...)
#endif

#endif

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

 /**
  * fixed and willing to merge with xine's solution ..
  *
  * sven goethel sgoethel@jausoft.com
  */

#include <assert.h>

#include <inttypes.h>
#include <malloc.h>
#include <string.h>

#include <vdr/tools.h>

#include "dvdspu.h"


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

cSPUassembler::cSPUassembler(): simpleFIFO(SPU_BUFFER_SIZE), spucmd(*this) { };

int cSPUassembler::Put(const uint8_t *Data, int Count, uint64_t Pts)
{
    if (Pts > 0)
	pts = Pts;
    return simpleFIFO::Put(Data, Count);
}

// ---------- cSPUCommand -----------------------------------

cSPUCommand::cSPUCommand(simpleFIFO &fifo)
: data(fifo)
{
    reset();
}

void cSPUCommand::reset()
{
    byte_pos=0;

    spu_size=data.Size();
    start_time=0;
    
    next_cs_offset=1;
    prev_cs_offset=0;
    eof_cs=0;

    command=CMD_SPU_EOF;

}

bool cSPUCommand::skipBytes(int num)
{
   if( availableBytes() < num )
   {
     esyslog("cSPUCommand::skipBytes: outta range: size=%d, num=%d, eaten=%d, avail=%d",
     	spu_size, num, eatenBytes(), availableBytes());
     DEBUG_CONTROL("cSPUCommand::skipBytes: outta range: size=%d, num=%d, eaten=%d, avail=%d\n",
     	spu_size, num, eatenBytes(), availableBytes());
     byte_pos=spu_size; // EOF
     return false;
   }
   byte_pos+=num;
   return true;
}

uint32_t cSPUCommand::getNextBytes( int num ) 
{
   uint32_t result = 0;

   if( num>4 ) 
   {
     esyslog("cSPUCommand::getNextBytes: used with more than 4 bytes");
     DEBUG_CONTROL("cSPUCommand::getNextBytes: used with more than 4 bytes");
     return( result );
   }

   if( availableBytes() < num )
   {
     esyslog("cSPUCommand::getNextBytes: outta range: size=%d, num=%d, eaten=%d, avail=%d",
     	spu_size, num, eatenBytes(), availableBytes());
     DEBUG_CONTROL("cSPUCommand::getNextBytes: outta range: size=%d, num=%d, eaten=%d, avail=%d\n",
     	spu_size, num, eatenBytes(), availableBytes());
     byte_pos=spu_size; // EOF
     return( result );
   }

   DEBUG_CONTROL("cSPUCommand::getNextBytes(%d) @ %d: ", num, byte_pos);
   uint8_t b;

   while( num > 0 ) {
     b = data[byte_pos];
     DEBUG_CONTROL(" 0x%X (%d)", b, b);
     result <<= 8;
     result |= b;
     byte_pos++;
     num--;
   }

   DEBUG_CONTROL(" : 0x%X (%d)\n", result, result);

   return( result );
}

bool cSPUCommand::next_cs_off()
{
    prev_cs_offset = next_cs_offset;
    next_cs_offset = getNextBytes(2);
    
    if ( next_cs_offset < 2 ) {
	esyslog("cSPUCommand::next_cs_off: offset < 2, error, size=%d, next_cs_offset=%d",
		spu_size, next_cs_offset);
	return false;
    }
    if ( next_cs_offset > spu_size ) {
	esyslog("cSPUCommand::next_cs_off: offset, too less data, size=%d, next_cs_offset=%d, d=%d",
		spu_size, next_cs_offset, next_cs_offset - spu_size);
	return false;
    }

    byte_pos += next_cs_offset - 2 ; // correct self containing 2 bytes ..

    eof_cs = getNextBytes(2);

    DEBUG_CONTROL("cSPUCommand::next_cs_off %u -> %u (eof_cs: %u)\n", prev_cs_offset, next_cs_offset, eof_cs);

    return true;
}

int cSPUCommand::initSPUCommand()
{
    reset();

    // first 2 bytes contains the spu size
    spu_size = getNextBytes(2);
    DEBUG_CONTROL("cSPUCommand::getSPUCommand SPU Size: %u\n", spu_size);
    
    return spu_size;
}
    
int cSPUCommand::getNextSPUCommand( )
{
    if( command!=CMD_SPU_EOF || prev_cs_offset != next_cs_offset )
    {
        if ( command==CMD_SPU_EOF )
	{
            if(!next_cs_off()) return -spu_size;
    
#if 0
	    // we use the eof_cs paradigma/semantics instead of start_time ..
	    //
	    if( availableBytes() >= 2 )
	    {
		// first 2 bytes of the control sequence contains the start time
		start_time = getNextBytes(2);
		DEBUG_CONTROL("cSPUCommand::getSPUCommand SPU StartTime: %u, off: %u\n", start_time, next_cs_offset);
	    } else {
		DEBUG_CONTROL("cSPUCommand::getSPUCommand SPU StartTime n.a.\n");
                return CMD_SPU_ERR;
	    }
#endif

        } else {
	    uint32_t param_len=0;

	    // jump to next cmd, using last command semantics ..
	    switch(command) 
	    {
		case CMD_SPU_SHOW:
		case CMD_SPU_HIDE:       
		case CMD_SPU_FORCE_DISPLAY:
			break;

		case CMD_SPU_WIPE:         
			if( availableBytes() >= 2 )
			   param_len = getNextBytes(2);
		        if(param_len>2)
				skipBytes(param_len-2);
			break;

		case CMD_SPU_SET_PALETTE:
			skipBytes(2);
			break;
		case CMD_SPU_SET_ALPHA: 
			skipBytes(2);
			break;
		case CMD_SPU_SET_SIZE: 
			skipBytes(6);
			break;
		case CMD_SPU_SET_PXD_OFFSET:
			skipBytes(4);
			break;
	    }
	}

        if( availableBytes() > 0 )
	   command = getNextBytes(1);
        else
	   command = CMD_SPU_EOF;
   } else {
        DEBUG_CONTROL("cSPUCommand::getSPUCommand NOP - FIN\n");
        command = CMD_SPU_EOF;
        return command;
   }

#ifdef CTRLDEBUG
   DEBUG_CONTROL("cSPUCommand::getSPUCommand Display Control Command: ");

   switch(command) 
   {
	case CMD_SPU_FORCE_DISPLAY:
		DEBUG_CONTROL("SPU FORCED\n");
		break;
	case CMD_SPU_SHOW:
		DEBUG_CONTROL("SPU SHOW\n");
		break;
	case CMD_SPU_HIDE:       
		DEBUG_CONTROL("SPU HIDE\n");
		break;
	case CMD_SPU_SET_PALETTE:
		DEBUG_CONTROL("SPU SET PAL\n");
		break;
	case CMD_SPU_SET_ALPHA: 
		DEBUG_CONTROL("SPU SET ALPHA\n");
		break;
	case CMD_SPU_SET_SIZE: 
		DEBUG_CONTROL("SPU SET SIZE\n");
		break;
	case CMD_SPU_SET_PXD_OFFSET:
		DEBUG_CONTROL("SPU PXD OFF\n");
		break;
	case CMD_SPU_WIPE:         
		DEBUG_CONTROL("SPU WIPE\n");
		break;
        case CMD_SPU_EOF:
		DEBUG_CONTROL("SPU EOF\n");
		break;
	default:
		DEBUG_CONTROL(" ??? : %d, 0x%X\n", command, command);
		break;
   }
#endif

   return command;
}

