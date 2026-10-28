
#ifndef _MPEGTYPES_H
#define _MPEGTYPES_H

#define SC_PICTURE 0x00

#define SYSTEM_HEADER    0xBB
#define PROG_STREAM_MAP  0xBC
#define PRIVATE_STREAM1  0xBD
#define PADDING_STREAM   0xBE
#define PRIVATE_STREAM2  0xBF
#define AUDIO_STREAM_S   0xC0
#define AUDIO_STREAM_E   0xDF
#define VIDEO_STREAM_S   0xE0
#define VIDEO_STREAM_E   0xEF

#define SEQUENCE_HEADER  0xb3
#define GOP_START        0xb8

#define I_FRAME 0x01
#define P_FRAME 0x02
#define B_FRAME 0x03

#define aAC3   0x80
#define aDTS   0x88
#define aLPCM  0xA0
#define aMPEG  0xC0

#endif
