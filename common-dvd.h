/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#ifndef __COMMON_DVD_H
#define __COMMON_DVD_H

//#define CTRLDEBUG
//#define DVDDEBUG
//#define PTSDEBUG
//#define AUDIOIDDEBUG
//#define AUDIOPLAYDEBUG
//#define NAVDEBUG
//#define SUBPDEBUG
//#define IFRAMEDEBUG

#ifdef DEBUG
    #undef  DEBUG
#endif

#ifdef DVDDEBUG
#define DEBUG(format, args...) printf (format, ## args)
#else
#define DEBUG(format, args...)
#endif
#ifdef PTSDEBUG
#define DEBUGPTS(format, args...) printf (format, ## args)
#else
#define DEBUGPTS(format, args...)
#endif
#ifdef AUDIOIDDEBUG
#define DEBUG_AUDIO_ID(format, args...) printf (format, ## args)
#else
#define DEBUG_AUDIO_ID(format, args...)
#endif
#ifdef CTRLDEBUG
#define DEBUG_CONTROL(format, args...) printf (format, ## args)
#else
#define DEBUG_CONTROL(format, args...)
#endif
#ifdef AUDIOPLAYDEBUG
#define DEBUG_AUDIO_PLAY(format, args...) printf (format, ## args)
#else
#define DEBUG_AUDIO_PLAY(format, args...)
#endif
#ifdef NAVDEBUG
#define DEBUG_NAV(format, args...) printf (format, ## args)
#else
#define DEBUG_NAV(format, args...)
#endif
#ifdef SUBPDEBUG
#define DEBUG_SUBP_ID(format, args...) printf (format, ## args)
#else
#define DEBUG_SUBP_ID(format, args...)
#endif

#ifdef IFRAMEDEBUG
#define DEBUG_IFRAME(format, args...) printf (format, ## args)
#else
#define DEBUG_IFRAME(format, args...)
#endif

#ifdef PTSDEBUG
#define DEBUG_PTS(format, args...) printf (format, ## args)
#else
#define DEBUG_PTS(format, args...)
#endif

#endif // __COMMON_DVD_H
