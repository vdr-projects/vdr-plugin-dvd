/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <byteswap.h>
#include "dvddev.h"
#include "setup-dvd.h"
#include "tools-dvd.h"
#include "player-dvd.h"
#include "ca52.h"

//#define A52DEBUG

#ifdef A52DEBUG
#warning A52DEBUG is defined
#define DEBUG(format, args...) fprintf (stderr, format, ## args)
#else
#define DEBUG(format, args...)
#endif

static const uint8_t burstHeader[4] = { 0xf8, 0x72, 0x4e, 0x1f };

A52decoder::A52decoder(cDvdPlayer &ThePlayer): player(ThePlayer)
{
  syncMode = ptsCopy;
  apts = 0;
  state = a52_init(MM_ACCEL_DJBFFT);
//  state = a52_init(MM_ACCEL_X86_MMXEXT);

  blk_buf = (uchar *)malloc(25*1024);
  blk_ptr = blk_buf;
  blk_size = 0;

  setup();
}

void A52decoder::setup(void)
{
  flags = A52_DOLBY;

  level = 4;
  bias = 384;
}

#define convert(x) x>0x43c07fff ? bswap_16(0x7fff) : x < 0x43bf8000 ? bswap_16(0x8000) : bswap_16(x - 0x43c00000);

inline void A52decoder::float_to_int (float * _f, int16_t * s16, int flags)
{
    int i;
    int32_t * f = (int32_t *) _f;

    switch (flags) {
        case A52_MONO: {
            int ic=0;
            for (i = 0; i < 256; i++) {
                s16[ic] = s16[ic+1] = s16[ic+2] = s16[ic+3] = 0;
                s16[ic+4] = convert (f[i]);
                ic+=5;
            }
            break;
        }
        case A52_CHANNEL:
        case A52_STEREO:
        case A52_DOLBY: {
            int ic=0;
            for (i = 0; i < 256; i++) {
                s16[ic] = convert (f[i]);
                s16[ic+1] = convert (f[i+256]);
                ic +=2;
            }
            break;
        }
        case A52_3F: {
            int ic=0;
            for (i = 0; i < 256; i++) {
                s16[ic] = convert (f[i]);
                s16[ic+1] = convert (f[i+512]);
                s16[ic+2] = s16[ic+3] = 0;
                s16[ic+4] = convert (f[i+256]);
                ic +=5;
            }
            break;
        }
        case A52_2F2R: {
            int ic=0;
            for (i = 0; i < 256; i++) {
                s16[ic] = convert (f[i]);
                s16[ic+1] = convert (f[i+256]);
                s16[ic+2] = convert (f[i+512]);
                s16[ic+3] = convert (f[i+768]);
                ic +=4;
            }
            break;
        }
        case A52_3F2R: {
            int ic=0;
            for (i = 0; i < 256; i++) {
                s16[ic] = convert (f[i]);
                s16[ic+1] = convert (f[i+256]);
                s16[ic+2] = convert (f[i+512]);
                s16[ic+3] = convert (f[i+768]);
                s16[ic+4] = convert (f[i+1024]);
                ic +=5;
            }
            break;
        }
        case A52_MONO | A52_LFE: {
            int ic=0;
            for (i = 0; i < 256; i++) {
                s16[ic] = s16[ic+1] = s16[ic+2] = s16[ic+3] = 0;
                s16[ic+4] = convert (f[i+256]);
                s16[ic+5] = convert (f[i]);
                ic +=6;
            }
            break;
        }
        case A52_CHANNEL | A52_LFE:
        case A52_STEREO | A52_LFE:
        case A52_DOLBY | A52_LFE: {
            int ic=0;
            for (i = 0; i < 256; i++) {
                s16[ic] = convert (f[i+256]);
                s16[ic+1] = convert (f[i+512]);
                s16[ic+2] = s16[ic+3] = s16[ic+4] = 0;
                s16[ic+5] = convert (f[i]);
                ic +=6;
            }
            break;
        }
        case A52_3F | A52_LFE: {
            int ic=0;
            for (i = 0; i < 256; i++) {
                s16[ic] = convert (f[i+256]);
                s16[ic+1] = convert (f[i+768]);
                s16[ic+2] = s16[ic+3] = 0;
                s16[ic+4] = convert (f[i+512]);
                s16[ic+5] = convert (f[i]);
                ic +=6;
            }
            break;
        }
        case A52_2F2R | A52_LFE: {
            int ic=0;
            for (i = 0; i < 256; i++) {
                s16[ic] = convert (f[i+256]);
                s16[ic+1] = convert (f[i+512]);
                s16[ic+2] = convert (f[i+768]);
                s16[ic+3] = convert (f[i+1024]);
                s16[ic+4] = 0;
                s16[ic+5] = convert (f[i]);
                ic +=6;
            }
            break;
        }
        case A52_3F2R | A52_LFE: {
            int ic=0;
            for (i = 0; i < 256; i++) {
                s16[ic] = convert (f[i+256]);
                s16[ic+1] = convert (f[i+768]);
                s16[ic+2] = convert (f[i+1024]);
                s16[ic+3] = convert (f[i+1280]);
                s16[ic+4] = convert (f[i+512]);
                s16[ic+5] = convert (f[i]);
                ic +=6;
            }
            break;
        }
    }
}

// data=PCM samples, 16 bit, LSB first, 48kHz, stereo
void A52decoder::init_ipack(int p_size, uint32_t pktpts)
{

#define PRIVATE_STREAM1  0xBD
#define aLPCM  0xA0

    int header = 0;

    if (pktpts != 0)
    header = 5;        // additional header bytes

    int length = 10 + header + p_size;

    blk_ptr[0] = 0x00;
    blk_ptr[1] = 0x00;
    blk_ptr[2] = 0x01;
    blk_ptr[3] = PRIVATE_STREAM1;
    blk_ptr[4] = (length >> 8) & 0xff;
    blk_ptr[5] = length & 0xff;
    blk_ptr[6] = 0x80;
    blk_ptr[7] = pktpts != 0 ? 0x80 : 0;
    blk_ptr[8] = header;
    blk_ptr += 9;

    if (header)
        cPStream::toPTS(blk_ptr, pktpts, false);
    blk_ptr += header;

    blk_ptr[0] = aLPCM; // substream ID
    blk_ptr[1] = 0x00;  // other stuff (see DVB specs), ignored by driver
    blk_ptr[2] = 0x00;
    blk_ptr[3] = 0x00;
    blk_ptr[4] = 0x00;
    blk_ptr[5] = 0x00;
    blk_ptr[6] = 0x00;
    blk_ptr += 7;

    blk_size += 16 + header;

}


int A52decoder::convertSample (int flags, a52_state_t * _state,
                uint32_t pktpts)
{
    int chans = -1;
    sample_t * _samples = a52_samples(_state);

#ifdef LIBA52_DOUBLE
    float samples[256 * 6];
    int i;

    for (i = 0; i < 256 * 6; i++)
    samples[i] = _samples[i];
#else
    float * samples = _samples;
#endif

    flags &= A52_CHANNEL_MASK | A52_LFE;

    if (flags & A52_LFE)
    chans = 6;
    else if (flags & 1) /* center channel */
    chans = 5;
    else switch (flags) {
    case A52_2F2R:
    chans = 4;
    break;
    case A52_CHANNEL:
    case A52_STEREO:
    case A52_DOLBY:
    chans = 2;
    break;
    default:
    return 1;
    }

    init_ipack((int) 256 * sizeof (int16_t) * chans, pktpts);
    float_to_int (samples, (int16_t *)blk_ptr, flags);
    // push data to driver
    blk_ptr  += (int) 256 * sizeof (int16_t) * chans;
    blk_size += (int) 256 * sizeof (int16_t) * chans;
    return 0;
}

void A52decoder::clear()
{
  DEBUG("A52decoder::clear()\n");
  blk_ptr = blk_buf;
  blk_size = 0;
  apts = 0;
  a52asm.clear();
}

/* from a52a.pdf:
 *
 * The AC-3 audio access unit (AU) or presentation unit (PU)
 * is an AC-3 sync frame. The AC-3 sync frame contains 1 536 audio
 * samples. The duration of an AC-3 access (or presentation) unit
 * is 32 ms for audio sampled at 48 kHz, approximately 34.83 ms for
 * audio sampled at 44.1 kHz, and 48 ms for audio sampled at 32 kHz.
 *
 * in 90kHz pts cycles:
 *   48kHz   = 2880
 *   44.1kHz = 3134.69
 *   32kHz   = 4320
 *
 * for now, lets assume we always have 48kHz!!
 */

void A52decoder::decode(uint8_t * start, int size,
             uint32_t pktpts)
{
    int bit_rate;

    //do we enter with an empty buffer
    bool sendPTS = a52asm.used() < 40;

    DEBUG("pts: %d %8d %8d, %8d\n",
      sendPTS, apts, pktpts, apts - pktpts);

    if (a52asm.used() == 0 || apts == 0 ||
    apts > (pktpts + 2880) || (apts + 5760) < pktpts) {
        DEBUG("reseting Audio PTS\n");
        apts = pktpts;
    }

    blk_ptr = blk_buf;
    blk_size = 0;

    while (size) {

    int res = a52asm.put(start, size, pktpts);
    start += res;
    size  -= res;
    if (a52asm.ready()) {

        int length;
        A52frame *frm = a52asm.get();
        if (!frm)
        goto error;
        length = a52_syncinfo (frm->frame,
                   &flags, &sample_rate, &bit_rate);
        if (!length) {
        delete frm;
        continue;
        }
        if (syncMode == ptsGenerate) {
        pktpts  = apts;
        sendPTS = true;
        }

        int i;

        setup();
        flags |= A52_ADJUST_LEVEL;
        if (a52_frame (state, frm->frame, &flags, &level, bias))
        goto error;
        if (!DVDSetup.AC3dynrng)
        a52_dynrng (state, NULL, NULL);
        for (i = 0; i < 6; i++) {
        if (a52_block (state))
            goto error;
        if (sendPTS) {
            player.seenPTS(pktpts);
            DEBUG("sending PTS\n");
        }
        if (convertSample (flags, state,
                   sendPTS ? pktpts : 0))
            goto error;
        pktpts  = 0;
        sendPTS = false;
        }
        apts += 2880;
        delete frm;
        continue;
    error:
        DEBUG("error\n");
        sendPTS = false;
        apts = 0;
        if (frm)
        delete frm;
    }
    }
    if (blk_size > 0)
    player.cbPlayAudio(blk_buf, blk_size);
}

A52frame::A52frame(int datasize, int frmsize, uint32_t apts)
{
    frame = new uint8_t[datasize+2];
    frame[0] = 0x0b;
    frame[1] = 0x77;
    size = frmsize;
    pos  = 2;
    pts  = apts;
}

A52frame::~A52frame()
{
    delete[] frame;
}

A52assembler::A52assembler()
{
    curfrm = NULL;
    syncword = 0xffff;
}

A52assembler::~A52assembler()
{
    if (curfrm)
    delete curfrm;
}

void A52assembler::clear(void)
{
    if (curfrm)
    delete curfrm;
    curfrm = NULL;
    syncword = 0xffff;
}

A52frame *A52assembler::get(void)
{
    A52frame *frm = curfrm;
    curfrm = NULL;
    syncword = 0xffff;

    return frm;
}

int A52assembler::used(void)
{
    if (curfrm) return curfrm->pos;
    return 0;
}

bool A52assembler::ready(void)
{
    return curfrm && curfrm->pos == curfrm->size;
};

#ifndef WORDS_BIGENDIAN
# define char2short(a,b)        ((((b) << 8) & 0xffff) ^ ((a) & 0xffff))
# define shorts(a)              (a)
#else
# define char2short(a,b)        ((((a) << 8) & 0xffff) ^ ((b) & 0xffff))
# define shorts(a)              char2short(((a) & 0xff),(((a) >> 8) & 0xff));
#endif

int A52assembler::put(uint8_t *buf, int len, uint32_t pts)
{
    if (ready())
    return 0;

    uint8_t *data = buf;
    uint8_t *end = data + len;

    while(syncword != char2short(0x77, 0x0b)) {
    if (data >= end) {
        DEBUG("skipping %d bytes without finding anything\n", data - buf);
        return data - buf;
    }
        syncword = (syncword << 8) | *data++;
    }

    int frmsize  = 0;
    if (!curfrm) {
    int datasize = 1920;
    if (end - data > 3)
        datasize = frmsize = parse_syncinfo(data);
    curfrm = new A52frame(datasize, frmsize, pts);
    }
    if (curfrm->size == 0) {
    if (curfrm->pos < 6) {
        frmsize = 6 - curfrm->pos;
    } else {
        curfrm->size = parse_syncinfo(curfrm->frame+2);
        frmsize = curfrm->size - curfrm->pos;
    }
    } else
    frmsize = curfrm->size - curfrm->pos;

    if (frmsize > end - data)
    frmsize = end - data;
    memcpy(curfrm->frame + curfrm->pos, data, frmsize);
    curfrm->pos += frmsize;

    return data + frmsize - buf;
}

// Parse a syncinfo structure, minus the sync word
int A52assembler::parse_syncinfo(uint8_t *data)
{
    static int rate[] = { 32,  40,  48,  56,  64,  80,  96, 112,
                         128, 160, 192, 224, 256, 320, 384, 448,
                         512, 576, 640};
    int frmsizecod;
    int bitrate;

    if (data[3] >= 0x60)         /* bsid >= 12 */
        return 0;

    frmsizecod = data[2] & 63;
    if (frmsizecod >= 38)
        return 0;
    bitrate = rate [frmsizecod >> 1];

    switch (data[2] & 0xc0) {
    case 0:
        return 4 * bitrate;
    case 0x40:
        return 2 * (320 * bitrate / 147 + (frmsizecod & 1));
    case 0x80:
        return 6 * bitrate;
    default:
        return 0;
    }
}


