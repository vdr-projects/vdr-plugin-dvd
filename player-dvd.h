/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#ifndef __PLAYER_DVD_H
#define __PLAYER_DVD_H

#include <dvdnav/dvdnav.h>
#include <dvdnav/dvdnav_events.h>

#include <vdr/device.h>
#include <vdr/player.h>
#include <vdr/ringbuffer.h>
#include <vdr/thread.h>
#include <vdr/dvbspu.h>

#if VDRVERSNUM<10307
    #define ERROR_MESSAGE(x) Interface->Error(tr(x));
#else
    #define ERROR_MESSAGE(x) Skins.Message(mtError, tr(x));
#endif

#include "common-dvd.h"

class cDvdPlayer;

#include "spu.h"
#include "ca52.h"

#define SYSTEM_HEADER    0xBB
#define SEQUENCE_HEADER  0xB3
#define PROG_STREAM_MAP  0xBC
#ifndef PRIVATE_STREAM1
#define PRIVATE_STREAM1  0xBD
#endif
#define PADDING_STREAM   0xBE
#ifndef PRIVATE_STREAM2
#define PRIVATE_STREAM2  0xBF
#endif
#define AUDIO_STREAM_S   0xC0
#define AUDIO_STREAM_E   0xDF
#define VIDEO_STREAM_S   0xE0
#define VIDEO_STREAM_E   0xEF
#define ECM_STREAM       0xF0
#define EMM_STREAM       0xF1
#define DSM_CC_STREAM    0xF2
#define ISO13522_STREAM  0xF3
#define PROG_STREAM_DIR  0xFF

#define I_TYPE       0x01

class IntegerListObject : public cListObject {
    private:
        int value;
    public:
        IntegerListObject() { value=0; }
        IntegerListObject( const IntegerListObject & v ) { value=v.value; }
        IntegerListObject( int i ) { value=i; }

        int getValue() const { return value; }
        void setValue(int v) { value=v; }

        virtual bool operator==(const cListObject & ListObject ) const
        { return value == ((IntegerListObject *)&ListObject)->value; }

        virtual bool operator<(const cListObject & ListObject ) /* const */
        { return value < ((IntegerListObject *)&ListObject)->value; }

        virtual bool operator>(const cListObject & ListObject ) const
        { return value > ((IntegerListObject *)&ListObject)->value; }

        operator int () const { return value; }
};

class cDvdPlayerControl ;

class cDvdPlayer : public cPlayer, cThread {
 private:
    enum ePlayModes { pmPlay, pmPause, pmSlow, pmFast, pmStill };
    enum ePlayDirs { pdForward, pdBackward };

    cDvdPlayerControl *controller;
    bool osdInUse;

    cSPUassembler SPUassembler;
    cSpuDecoder *SPUdecoder;
    A52decoder a52dec;
#ifdef NOT_YET
    cDTSdecoder dtsdec;
#endif

    int hsize;
    int vsize;
    int vaspect;

    cRingBufferLinear *iframeAssembler;
    int IframeCnt;

    cRingBufferFrame *ringBuffer;
    cFrame *rframe, *pframe;

    dvdnav_cell_change_event_t lastCellEventInfo ;
    int pgcTotalBlockNum;
    int64_t pgcTotalTicks;
    int64_t pgcTicksPerBlock;

    int skipPlayVideo;
    int fastWindFactor;
    uint32_t cntBlocksPlayed;

    int currentNavSubpStream;
    bool currentNavSubpStreamLocked;
    bool spuInUse;
    bool forcedSubsOnly, spu_state;
    
    cList<IntegerListObject> navSubpStreamSeen;
    void notifySeenSubpStream( int navSubpStream );
    void clearSeenSubpStream( );

    uint32_t stillTimer;
    dvdnav_t *nav;
    pci_t *current_pci;
    int prev_e_ptm;
    int ptm_offs;

    uint8_t stillFrame;
    uint8_t lastFrameType;
    uint32_t VideoPts;
    uint32_t stcPTS;
    uint32_t stcPTSLast;
    uint32_t pktpts;
    uint32_t pktptsLast;
    uint32_t stcPTSAudio;
    uint32_t stcPTSLastAudio;
    uint32_t pktptsAudio;
    uint32_t pktptsLastAudio;

    int   currentNavAudioTrack;
    int   currentNavAudioTrackType; // aAC3, aDTS, aLPCM, aMPEG
    bool  currentNavAudioTrackLocked;

    cList<IntegerListObject> navAudioTracksSeen;
    void notifySeenAudioTrack( int navAudioTrack );
    void clearSeenAudioTrack( );

    static int Speeds[];
    bool running;
    bool active;
    ePlayModes playMode;
    ePlayDirs playDir;
    int trickSpeed;

    //player stuff
    void TrickSpeed(int Increment);
    void Empty(bool emptyDeviceToo=true);
    void StripAudioPackets(uchar *b, int Length, uchar Except = 0x00);
    int Resume(int lastBlocksPlayed);
    bool Save(const char * diskStamp, int arrayIndex);
    bool setDiskStamp(const char ** stamp_str, dvdnav_t * nav) const;
    void checkDiskStamps(const char * stamp_str, int &lastTitle, int &lastBlocks, int &lastArrayIndex);
    bool askForResume(int blocks);
    static bool BitStreamOutActive;
    static bool HasBitStreamOut;

    //dvd stuff
    int currButtonN;
    void ClearButtonHighlight(void);
    void UpdateButtonHighlight(void);
    cSpuDecoder::eScaleMode doScaleMode();
    cSpuDecoder::eScaleMode getSPUScaleMode();
    void SendIframe(bool doSend);

    void playPacket(unsigned char *&cache_buf, bool trickMode, bool noAudio);

 protected:
    virtual void Activate(bool On);
    virtual void Action(void);
    void DeviceClear(void);

    uint32_t time_ticks(void);
    uint32_t delay_ticks(uint32_t ticks);

 public:

    static const int MaxAudioTracks ;
    static const int MaxSubpStreams;

    cDvdPlayer(void);
    virtual ~cDvdPlayer();
    bool Active(void) const { return active ; }
    bool DVDActiveAndRunning(void) const { return active && nav!=NULL && running; }

    // -- control stuff --
    void setController (cDvdPlayerControl *ctrl );
    bool IsDvdNavigationForced() const ;

    void Pause(void);
    void Play(void);
    void Stop(void);
    void Forward(void);
    void Backward(void);
    bool GetReplayMode(bool &Play, bool &Forward, int &Speed) ;
    void GetTitleString( const char ** title_str ) const ;
    void GetTitleInfoString( const char ** title_str ) const ;
    void GetAspectString( const char ** aspect_str ) const ;
    int GetProgramNumber() const ;
    int GetCellNumber() const ;

    /**
     * returns the PG (e.g. chapter) lenght in ticks
     *
     * 90000 ticks are 1 second, acording to MPEG !
     */
    int64_t GetPGLengthInTicks() ;

    /**
     * returns the PGC (e.g. whole title) lenght in ticks
     *
     * 90000 ticks are 1 second, acording to MPEG !
     */
    int64_t GetPGCLengthInTicks() ;

    /**
     * returns the CurrentTicks and TotalTicks according to the given
     * CurrentBlockNum, TotalBlockNum and actual PGC !
     *
     * if TotalBlockNum equals -1, the Total BlockNumber of the actual PGC is fetched !
     *
     * 90000 ticks are 1 second, acording to MPEG !
     */
    void BlocksToPGCTicks( int CurrentBlockNum, int TotalBlockNum,
                   int64_t & CurrentTicks, int64_t & TotalTicks ) ;

    /**
     * returns the CurrentBlockNum and TotalBlockNum according to the given
     * CurrentTicks, TotalTicks and actual PGC !
     *
     * if TotalTicks equals -1, the Total Ticks of the actual PGC is fetched !
     *
     * 90000 ticks are 1 second, acording to MPEG !
     */
    void PGCTicksToBlocks( int64_t CurrentTicks, int64_t TotalTicks,
               int &CurrentBlockNum, int &TotalBlockNum) ;

    /**
     * returns the CurrentBlockNum and TotalBlockNum of the actual PGC !
     *
     * optional snap to the next IFrame (not functional now)
     */
    bool GetBlockIndex(int &CurrentBlockNum, int &TotalBlockNum, bool SnapToIFrame=false) ;

    /**
     * returns the CurrentFrame and TotalFrame of the actual PGC !
     * the timebase is accurate (BlockNumber/Ticks), but the resulting
     * "frame" number is calculated using the magic number FRAMESPERSEC ;-)
     *
     * optional snap to the next IFrame (not functional now)
     */
    bool GetIndex(int &CurrentFrame, int &TotalFrame, bool SnapToIFrame) ;

    bool GetPositionInSec(int64_t &CurrentSec, int64_t &TotalSec) ;
    bool GetPositionInTicks(int64_t &CurrentTicks, int64_t &TotalTicks) ;

    int  getWidth()  const { return hsize; }
    int  getHeight() const { return vsize; }
    int  getAspect() const { return vaspect; }

    /**
     * we assume FRAMESPERSEC !
     */
    int  SkipFrames(int Frames);
    void SkipSeconds(int Seconds);

    /**
     * set the position of the actual PGC to the given Seconds
     */
    void Goto(int Seconds, bool Still = false);

    int GotoAngle(int angle);

    /**
     * jump to the next Angle (rotate)
     */
    void NextAngle();

    /**
     * jump to the next Title (rotate)
     */
    void NextTitle();

    /**
     * goto to the Title
     *
     * return set title ..
     */
    int GotoTitle(int title);

    /**
     * jump to the previous Title (rotate)
     */
    void PreviousTitle();

    /**
     * goto to the Part
     *
     * return set Part ..
     */
    int GotoPart(int part);

    /**
     * jump to the next Part (rotate)
     */
    void NextPart();

    /**
     * jump to the previous Part (rotate)
     */
    void PreviousPart();

    /**
     * jump to the next PG
     */
    void NextPG();

    /**
     * jump to the previous PG
     */
    void PreviousPG();

    bool IsSPUInUse(void) { return spuInUse; }
    bool GetCurrentNavSubpStreamLocked(void) const ;
    void SetCurrentNavSubpStreamLocked(bool lock) ;
    int  GetCurrentNavSubpStream(void) const ;
    int  GetCurrentNavSubpStreamIdx(void) const ;
    int  GetNavSubpStreamNumber (void) const ;
    int  SetSubpStream(int id);
    void NextSubpStream();
    void GetSubpLangCode( const char ** subplang_str ) const ;

    /**
     * set the AudioID
     *
     * return set audio id ..
     */
    int SetAudioID(int id);

    /**
     * jump to the next audio id (rotate)
     */
    void NextAudioID();

    bool GetCurrentNavAudioTrackLocked(void) const ;
    void SetCurrentNavAudioTrackLocked(bool lock);
    int  GetNavAudioTrackNumber (void) const ;
    int  GetCurrentNavAudioTrack(void) const ;
    int  GetCurrentNavAudioTrackIdx(void) const ;
    int  GetCurrentNavAudioTrackType(void) const ; // aAC3, aDTS, aLPCM, aMPEG
    void GetAudioLangCode( const char ** audiolang_str ) const ;

    virtual void SetAudioTrack(int Index) { (void) SetAudioID( Index ); }
    virtual int NumAudioTracks(void) const { return GetNavAudioTrackNumber(); }
    virtual const char **GetAudioTracks(int *CurrentTrack = NULL) const ;

    void selectUpButton(void);
    void selectDownButton(void);
    void selectLeftButton(void);
    void selectRightButton(void);
    void activateButton(void);

    int callRootMenu(void);
    int callTitleMenu(void);
    int callSubpMenu(void);
    int callAudioMenu(void);

    bool IsInMenuDomain();
    bool IsInStillFrame();
    bool IsInMenuDomainOrStillFrame();

    // -- callbacks --
    void seenPTS(uint32_t pts);

    int cbPlayVideo(uchar *Data, int Length);
    int cbPlayAudio(uchar *Data, int Length);

    /**
     * 90000 ticks are 1 second, acording to MPEG !
     */
    static const char *PTSTicksToHMSm(int64_t ticks, bool WithMS=false);

    static int64_t PTSTicksToMilliSec(int64_t ticks)
    { return ticks / 90L; }

    static int64_t PTSTicksToSec(int64_t ticks)
    { return ticks / 90000L; }

};

#define CIF_MAXSIZE  256

// --- cDvdPlayer ---------------------------------------------------

inline bool cDvdPlayer::IsInMenuDomain() {
    return running & nav != NULL &&
    (dvdnav_is_domain_vmgm(nav) || dvdnav_is_domain_vtsm(nav));
}
inline bool cDvdPlayer::IsInStillFrame()
{
    return stillFrame!=0;
}

inline bool cDvdPlayer::IsInMenuDomainOrStillFrame()
{
    return IsInStillFrame()||IsInMenuDomain();
}

inline int cDvdPlayer::cbPlayVideo(uchar *Data, int Length)
{
    rframe = new cFrame(Data, Length, ftVideo);
    if( rframe && ringBuffer && ringBuffer->Put(rframe) ) rframe=0;
    return Length;
}

inline int cDvdPlayer::cbPlayAudio(uchar *Data, int Length)
{
    rframe = new cFrame(Data, Length, ftAudio);
    if( rframe && ringBuffer && ringBuffer->Put(rframe) ) rframe=0;
    return Length;
}

// --- navigation stuff ---

inline void cDvdPlayer::selectUpButton(void)
{
    DEBUG("selectUpButton\n");
    if (current_pci) {
      dvdnav_upper_button_select(nav, current_pci);
    }
}

inline void cDvdPlayer::selectDownButton(void)
{
    DEBUG("selectDownButton\n");
    if (current_pci) {
      dvdnav_lower_button_select(nav, current_pci);
    }
}

inline void cDvdPlayer::selectLeftButton(void)
{
    DEBUG("selectLeftButton\n");
    if (current_pci) {
      dvdnav_left_button_select(nav, current_pci);
    }
}

inline void cDvdPlayer::selectRightButton(void)
{
    DEBUG("selectRightButton\n");
    if (current_pci) {
      dvdnav_right_button_select(nav, current_pci);
    }
}

inline void cDvdPlayer::activateButton(void)
{
    DEBUG("activateButton\n");
    if (current_pci)
      dvdnav_button_activate(nav, current_pci);
}

inline int cDvdPlayer::callRootMenu(void)
{
    DEBUG("cDvdPlayer::callRootMenu()\n");

    LOCK_THREAD;

    SetCurrentNavAudioTrackLocked(false);
    return dvdnav_menu_call(nav, DVD_MENU_Root);
}

inline int cDvdPlayer::callTitleMenu(void)
{
    DEBUG("cDvdPlayer::callTitleMenu()\n");

    LOCK_THREAD;

    SetCurrentNavAudioTrackLocked(false);
    return dvdnav_menu_call(nav, DVD_MENU_Title);
}

inline int cDvdPlayer::callSubpMenu(void)
{
    DEBUG("cDvdPlayer::callSubpMenu()\n");

    LOCK_THREAD;

    return dvdnav_menu_call(nav, DVD_MENU_Subpicture);
}

inline int cDvdPlayer::callAudioMenu(void)
{
    DEBUG("cDvdPlayer::callAudioMenu()\n");

    LOCK_THREAD;

    SetCurrentNavAudioTrackLocked(false);
    return dvdnav_menu_call(nav, DVD_MENU_Audio);
}

#endif //__PLAYER_DVD_H

