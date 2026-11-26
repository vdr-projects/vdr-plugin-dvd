#ifndef VDR_DVD_MPEGTYPES_H
#define VDR_DVD_MPEGTYPES_H

// The start codes of MPEG video: picture, sequence header and group of pictures.
inline constexpr int SC_PICTURE = 0x00;
inline constexpr int SEQUENCE_HEADER = 0xb3;
inline constexpr int GOP_START = 0xb8;

// The stream ids of the MPEG program stream.
inline constexpr int SYSTEM_HEADER = 0xBB;
inline constexpr int PROG_STREAM_MAP = 0xBC;
inline constexpr int PRIVATE_STREAM1 = 0xBD;
inline constexpr int PADDING_STREAM = 0xBE;
inline constexpr int PRIVATE_STREAM2 = 0xBF;
inline constexpr int AUDIO_STREAM_S = 0xC0;
inline constexpr int AUDIO_STREAM_E = 0xDF;
inline constexpr int VIDEO_STREAM_S = 0xE0;
inline constexpr int VIDEO_STREAM_E = 0xEF;

// The picture coding types of an MPEG video picture.
inline constexpr int I_FRAME = 0x01;
inline constexpr int B_FRAME = 0x03;

// The substream ids of the DVD's audio in private stream 1, track 0.
inline constexpr int aAC3 = 0x80;
inline constexpr int aDTS = 0x88;
inline constexpr int aLPCM = 0xA0;
inline constexpr int aMPEG = 0xC0;

#endif // VDR_DVD_MPEGTYPES_H
