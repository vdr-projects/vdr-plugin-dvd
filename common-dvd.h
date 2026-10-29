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

// Debug output on stdout, one switch per area.
//#define CTRLDEBUG
//#define DVDDEBUG
//#define PTSDEBUG
//#define PTSDEBUG2
//#define AUDIOIDDEBUG
//#define AUDIOPLAYDEBUG2
//#define NAVDEBUG
//#define NAV0DEBUG
//#define SUBPDEBUG
//#define IFRAMEDEBUG
//#define IFRAMEDEBUG2
//#define KEYDEBUG
//#define SHOWDEBUG
//#define OSDCTRLDEBUG
// Writes the I-frame of each still picture to /tmp/dvd.iframe.NNN.asm.pes.
//#define IFRAMEWRITE

#ifdef DVDDEBUG
#define DEBUGDVD(...) printf(__VA_ARGS__)
#else
#define DEBUGDVD(...)
#endif
#ifdef AUDIOIDDEBUG
#define DEBUG_AUDIO_ID(...) printf(__VA_ARGS__)
#else
#define DEBUG_AUDIO_ID(...)
#endif
#ifdef CTRLDEBUG
#define DEBUG_CONTROL(...) printf(__VA_ARGS__)
#else
#define DEBUG_CONTROL(...)
#endif
#ifdef AUDIOPLAYDEBUG2
#define DEBUG_AUDIO_PLAY2(...) printf(__VA_ARGS__)
#else
#define DEBUG_AUDIO_PLAY2(...)
#endif
#ifdef NAVDEBUG
#define DEBUG_NAV(...) printf(__VA_ARGS__)
#else
#define DEBUG_NAV(...)
#endif
#ifdef NAV0DEBUG
#define DEBUG_NAV0(...) (printf(__VA_ARGS__), fflush(NULL))
#else
#define DEBUG_NAV0(...)
#endif
#ifdef SUBPDEBUG
#define DEBUG_SUBP_ID(...) printf(__VA_ARGS__)
#else
#define DEBUG_SUBP_ID(...)
#endif

#ifdef IFRAMEDEBUG
#define DEBUG_IFRAME(...) printf(__VA_ARGS__)
#else
#define DEBUG_IFRAME(...)
#endif
#ifdef IFRAMEDEBUG2
#define DEBUG_IFRAME2(...) printf(__VA_ARGS__)
#else
#define DEBUG_IFRAME2(...)
#endif

#ifdef PTSDEBUG
#define DEBUG_PTS(...) printf(__VA_ARGS__)
#else
#define DEBUG_PTS(...)
#endif
#ifdef PTSDEBUG2
#define DEBUG_PTS2(...) printf(__VA_ARGS__)
#else
#define DEBUG_PTS2(...)
#endif

#ifdef KEYDEBUG
#define DEBUG_KEY(...) (printf(__VA_ARGS__), fflush(NULL))
#else
#define DEBUG_KEY(...)
#endif
#ifdef SHOWDEBUG
#define DEBUG_SHOW(...) (printf(__VA_ARGS__), fflush(NULL))
#else
#define DEBUG_SHOW(...)
#endif
#ifdef OSDCTRLDEBUG
#define DEBUG_OSDCTRL(...) (printf(__VA_ARGS__), fflush(NULL))
#else
#define DEBUG_OSDCTRL(...)
#endif

#endif // __COMMON_DVD_H
