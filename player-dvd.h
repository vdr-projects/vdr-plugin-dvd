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

#include <stdarg.h>
#include <dvdnav/dvdnav.h>
#include <dvdnav/dvdnav_events.h>

// dvdnav.h includes version.h since libdvdnav 6.1.0. Before, there was no
// DVDNAV_VERSION_CODE, and 5.x had no DVDNAV_VERSION either (0 in #if).
#ifndef DVDNAV_VERSION_CODE
#define DVDNAV_VERSION_CODE(major, minor, micro) ((major) * 10000 + (minor) * 100 + (micro))
#endif

#include <atomic>
#include <vdr/device.h>
#include <vdr/player.h>
#include <vdr/ringbuffer.h>
#include <vdr/thread.h>
#include <vdr/dvbspu.h>

#include "common-dvd.h"

class cDvdPlayer;

#include "dvdspu.h"
#include "ca52.h"

class IntegerListObject : public cListObject {
    private:
        int value;
    public:
        IntegerListObject( int i ) { value=i; }

        int getValue() const { return value; }
};

class cDvdPlayerControl ;
class cIframeAssembler;
class cDVDPlayerResume;

class cDvdPlayer : public cPlayer, cThread {
 private:

    enum ePlayModes { pmPlay, pmPause, pmSlow, pmFast, pmStill };
    enum ePlayDirs { pdForward, pdBackward };
    enum ePacketType { pktNone=0, pktVideo=1, pktAudio=2, pktIFrame=4, pktSpu=8 };

    cDvdPlayerControl *controller;

    cSPUassembler SPUassembler;
    cSpuDecoder *SPUdecoder;
    A52decoder a52dec;

    cIframeAssembler *iframeAssembler;
    int IframeCnt;
    uint8_t * event_buf;

    cRingBufferFrame *ringBuffer;
    cFrame *rframe, *pframe;

    dvdnav_cell_change_event_t lastCellEventInfo ;
    uint32_t pgcCurrentBlockNum, pgcTotalBlockNum;
    int64_t pgcTotalTicks;
    int64_t pgcTicksPerBlock;
    bool UpdateBlockInfo(void);

    bool isInFeature;
    int dvd_aspect;
    int dvd_scaleperm;
    void UpdateVTSInfo(void);

    bool skipPlayVideo;
    uchar videoPictureType; // of the last picture header, 0 = none since Empty()
    int fastWindFactor;
    uint32_t cntVidBlocksPlayed;
    uint32_t cntAudBlocksPlayed;

    // Subpicture streams have a logical number, which the disc's attributes,
    // its menus and VDR's track list use, and a physical one in the VOB for
    // each display mode.
    int currentNavSubpStream;   // logical, bit 7 set: forced subtitles only; -1 = none
    int currentNavSubpPhysical; // the physical stream shown for it, -1 = none
    uint32_t subpPhysicalSeen;  // physical streams whose packets turned up
    bool currentNavSubpStreamUsrLocked;
    bool changeNavSubpStreamOnceInSameCell;

    cList<IntegerListObject> navSubpStreamSeen;
    void clearSeenSubpStream(void);
    void setAllSubpStreams(void);
    int SearchSubpStream(int SubpStreamId) const;
    void notifySeenSubpStream( int navSubpStream );
    void SelectSubpStream(void);
    void SetSubpStream(int id);
    void SetCurrentNavSubpStreamUsrLocked(bool lock) ;

    uint64_t stillTimer; // in time_ticks(), 0 = no still frame
    dvdnav_t *nav;
    pci_t *current_pci;
    int prev_e_ptm;
    int ptm_offs;

    // The VOBUs read last, to find the one on screen: ReplayFromPicture().
    struct cVobuPosition {
        uint32_t start, end; // its pictures' PTS as the device gets them
        uint32_t block;      // position in the program chain
        int32_t title, pgcN;
    };
    enum { VobuHistorySize = 32 }; // > 10 s, more than any device holds
    cVobuPosition vobuHistory[VobuHistorySize];
    int vobuHistoryCount, vobuHistoryNext;
    bool replayFromPictureOnPlay;
    void NoteVobu(const pci_t *Pci);
    bool ReplayFromPicture(void);

    uint8_t stillFrame;
    uint8_t lastFrameType;
    uint64_t VideoPts;
    uint64_t pktpts;

    int   currentNavAudioTrack;
    int   currentNavAudioTrackType; // aAC3, aDTS, aLPCM, aMPEG
    bool  currentNavAudioTrackUsrLocked;

    cList<IntegerListObject> navAudioTracksSeen;
    void clearSeenAudioTrack(void);
    void setAllAudioTracks(void);
    void notifySeenAudioTrack(int navAudioTrack);
    int SearchAudioStream(int AudioStreamId) const;
    void SetCurrentNavAudioTrackType(int atype);

    static int Speeds[];
    std::atomic<bool> running;
    std::atomic<bool> active;
    std::atomic<bool> navFailed; // see NavLog()
#if DVDNAV_VERSION >= DVDNAV_VERSION_CODE(6, 1, 0)
    static void NavLog(void *Player, dvdnav_logger_level_t Level, const char *Format, va_list Args);
    cMutex navLogMutex; // libdvdnav logs from whatever thread calls it
    cStringList navLogged; // the notes NavLog() logged
    bool NavLogFirst(const char *Msg);
#endif
    std::atomic<ePlayModes> playMode; // the thread's loop checks it without the lock
    ePlayDirs playDir;
    int trickSpeed;

    static const char * dummy_title;
    static const char * dummy_n_a;
    char * title_str;
    char * titleinfo_str;
    char * aspect_str;
    void SetTitleString( void ) ;
    void SetTitleInfoString( void );
    void SetAspectString( void );

    // What the control shows, kept up to date by the player thread: the
    // control polls it from VDR's main loop, which must not wait for the
    // thread's lock while the thread reads the disc or drains the device.
    mutable cMutex infoMutex;
    bool infoPositionValid;
    uint32_t infoCurrentBlock, infoTotalBlock;
    int64_t infoPgcLength;
    char infoAudio[100];
    char infoSubtitle[100];
    std::atomic<bool> menuButtons; // IsInMenuDomain()
    void ReplaceInfoString(char *&Str, char *New);
    void UpdateTrackInfo(void);

    //player stuff
    void StillSkip(void);
    void TrickSpeed(int Increment);
    void Empty(bool emptyDeviceToo=true);
    void PlayFromBlock(int64_t Block);

    //dvd stuff
    void ClearButtonHighlight(void);
    void UpdateButtonHighlight(dvdnav_highlight_event_t *hlevt);
    void DoScaleMode(int &vaspect);
    cSpuDecoder::eScaleMode getSPUScaleMode();
    void SendIframe(bool doSend);

    int  playPacket(unsigned char *&cache_buf, bool trickMode, bool noAudio);
    void SetAvailableAudioTrack(eTrackType Type, int Index, uint16_t Id);
    int announcedLanguageCode[ttMaxTrackTypes]; // per track, -1 = not announced
    eTrackType currentAudioTrack; // VDR's current audio track, as set by the player or VDR
    // An audio track VDR selects from another thread, for the player thread
    // to switch to, see SetAudioTrack():
    cMutex audioTrackRequestMutex;
    eTrackType audioTrackRequest; // ttNone: none
    bool audioTrackRequestByViewer;
    std::atomic<tThreadId> playerThreadId;
    void SwitchAudioTrack(eTrackType Type, bool ByViewer);
    void SwitchRequestedAudioTrack(void);
    void playSPU(int spuId);

    static int DolbyTrackIndex(int SubStreamId);
    bool subtitleTrackSync; // the player announces or reports a subtitle track to VDR
    void AnnounceSubtitleTrack(int Stream);
    void ReportSubtitleTrack(void);
    bool subpMenuChoice; // the viewer used a button of the disc's menus
    bool SubtitlesShown(int Stream) const;
    int  LogicalAudioStream(int Physical) const;
    int  LogicalSubpStream(int Physical) const;
    int  PhysicalSubpStream(int Logical) const;
    uint16_t GetAudioTrackLanguageCode(int channel) const;
    cString AudioTrackDescription(int Channel, const char *LanguageName) const;
    cString SubtitleTrackDescription(int Channel, const char *LanguageName) const;
    int  GetAudioStreamNumbers(void) const ;
    uint16_t GetSubtitleLanguageCode(int Channel) const;
    int  GetSubtitleStreamNumbers(void) const ;

    //resuming
    /**
     * the resume database
     */
    cDVDPlayerResume* resume;
    /**
     * GetDVDResumeKey():
     * computes a (hopefully) unique id for storing the resume data of the current disc.
     *
     * this get returns a new allocated memory area ..
     * must be freed by callee ..
     */
    char* GetDVDResumeKey() const;
    /**
     * SaveResume():
     * handles everything to save the current position on the disc for later resuming.
     */
    void SaveResume();
    /**
     * LoadResume():
     * loads the resume data for the current disc and stores it in the given arguments.
     * returns false if no resume data for the disc can be found or and error occured while loading.
     */
    bool LoadResume(int& title, int& chapter, int64_t& second);

protected: //Player
    virtual void Activate(bool On);
    virtual void Action(void);
    void DeviceReset(void);
    void DeviceClear(void);
    void DevicePlay(void);
    bool WaitDeviceDrained(void);
    int emptyCount; // Empty() calls, for WaitDeviceDrained()

public:
    /**
     * returns the CurrentFrame and TotalFrame of the actual PGC !
     * the timebase is accurate (BlockNumber/Ticks), but the resulting
     * "frame" number is calculated using the magic number FRAMESPERSEC ;-)
     *
     * optional snap to the next IFrame (not functional now)
     */
    bool GetIndex(int &CurrentFrame, int &TotalFrame, bool SnapToIFrame);
    bool GetReplayMode(bool &Play, bool &Forward, int &Speed) ;
    void SetAudioTrack(eTrackType Type, const tTrackId *TrackId);
    virtual void SetSubtitleTrack(eTrackType Type, const tTrackId *TrackId);

protected:
    void seenSpuPts(uint64_t pts);
    uint64_t time_ticks(void);
    void delay_ticks(uint64_t ticks);
public:

    static const int MaxAudioTracks;
    static const int AudioTrackMask;
    static const int AC3AudioTrackMask;
    static const int MaxSubpStreams;
    static const int SubpStreamMask;

    cDvdPlayer(void);
    virtual ~cDvdPlayer();
    bool Active(void) const { return active ; }
    bool DVDActiveAndRunning(void) const { return active && nav != NULL && running; }
    bool DVDRemoveable(void) const { return !active && !running; }
    void GetAudioLanguageStr(const char **AudioLanguageStr) const ;
    void GetSubtitleLanguageStr( const char **SubtitleLanguageStr ) const ;

    // -- control stuff --
    void setController (cDvdPlayerControl *ctrl );
    bool IsDvdNavigationForced() const ;
    void DrawSPU();
    void HideSPU();
    void EmptySPU();

    void Pause(void);
    void Play(void);
    void Stop(void);
    void Forward(void);
    void Backward(void);

    /**
     * these getter returns a new allocated memory area ..
     * must be freed by callee ..
     */
    char *GetTitleString(void) const ;
    char *GetTitleInfoString(void) const ;
    char *GetAspectString(void) const ;

    /**
     * returns the PGC (e.g. whole title) lenght in ticks
     *
     * 90000 ticks are 1 second, acording to MPEG !
     */
    int64_t GetPGCLengthInTicks(void) ;

    /**
     * returns the Ticks and TotalTicks according to the given
     * BlockNum, TotalBlockNum of the actual PGC !
     *
     * the TotalBlockNum of the actual PGC is fetched !
     *
     * 90000 ticks are 1 second, acording to MPEG !
     */
    void BlocksToPGCTicks(uint32_t BlockNum, int64_t &Ticks, int64_t &TotalTicks) ;

    /**
     * returns the CurrentBlockNum and TotalBlockNum according to the given
     * Ticks, TotalTicks of the actual PGC !
     *
     * the TotalTicks of the actual PGC is fetched !
     *
     * 90000 ticks are 1 second, acording to MPEG !
     */
    void PGCTicksToBlocks(int64_t Ticks, int64_t &BlockNum, uint32_t &TotalBlockNum) ;

    bool GetPositionInSec(int64_t &CurrentSec, int64_t &TotalSec) ;
    void GetPositionInTicks(int64_t &CurrentTicks, int64_t &TotalTicks) ;

    /**
     * we assume FRAMESPERSEC !
     */
    int  SkipFrames(int Frames);
    void SkipSeconds(int Seconds);

    /**
     * set the position of the actual PGC to the given Seconds
     */
    void Goto(int Seconds);

    int GotoAngle(int Angle);

    /**
     * jump to the next Angle (rotate)
     */
    void NextAngle(void);

    /**
     * jump to the next Title (rotate)
     */
    void NextTitle(void);

    /**
     * goto to the Title
     *
     * return set title ..
     */
    // GotoTitle now optionally takes a chapter to seek to in the given title
    int GotoTitle(int Title, int Chapter = 1);

    /**
     * jump to the previous Title (rotate)
     */
    void PreviousTitle(void);

    /**
     * goto to the Part
     *
     * return set Part ..
     */
    int GotoPart(int Part);

    /**
     * jump to the next Part (rotate)
     */
    void NextPart(void);

    /**
     * jump to the previous Part (rotate)
     */
    void PreviousPart(void);

    int  GetCurrentNavSubpStream(void) const ;
    void NextSubpStream();

    void SetCurrentNavAudioTrackUsrLocked(bool lock);
    int  GetCurrentNavAudioTrackIdx(void) const ;


    void selectUpButton(void);
    void selectDownButton(void);
    void selectLeftButton(void);
    void selectRightButton(void);
    void activateButton(void);

    int callRootMenu(void);
    int callTitleMenu(void);
    int callSubpMenu(void);
    int callAudioMenu(void);

    bool IsInMenuDomain() const;

    // -- callbacks --
    void seenAPTS(uint64_t pts);
    void seenVPTS(uint64_t pts);

    int cbPlayAudio(uchar *Data, int Length);

    /**
     * 90000 ticks are 1 second, acording to MPEG !
     */
    static int64_t PTSTicksToSec(int64_t ticks)
    { return ticks / 90000L; }

};

#define CIF_MAXSIZE  512

// --- cDvdPlayer ---------------------------------------------------

inline bool cDvdPlayer::IsInMenuDomain() const
{
    // current_pci->hli.hl_gi.hli_ss as of the last NAV packet; current_pci
    // points into nav, which the player thread closes when it ends
    return menuButtons;
}

inline int cDvdPlayer::cbPlayAudio(uchar *Data, int Length)
{
    if (ringBuffer) {
        cFrame *cbFrame = new cFrame(Data, Length, ftDolby);
        if(cbFrame && !ringBuffer->Put(cbFrame))
            DELETENULL(cbFrame);
    }
    return Length;
}

// --- navigation stuff ---

inline void cDvdPlayer::selectUpButton(void)
{
    DEBUGDVD("selectUpButton\n");
    LOCK_THREAD;
    if (nav && current_pci) {
        dvdnav_upper_button_select(nav, current_pci);
    }
}

inline void cDvdPlayer::selectDownButton(void)
{
    DEBUGDVD("selectDownButton\n");
    LOCK_THREAD;
    if (nav && current_pci) {
        dvdnav_lower_button_select(nav, current_pci);
    }
}

inline void cDvdPlayer::selectLeftButton(void)
{
    DEBUGDVD("selectLeftButton\n");
    LOCK_THREAD;
    if (nav && current_pci) {
        dvdnav_left_button_select(nav, current_pci);
    }
}

inline void cDvdPlayer::selectRightButton(void)
{
    DEBUGDVD("selectRightButton\n");
    LOCK_THREAD;
    if (nav && current_pci) {
        dvdnav_right_button_select(nav, current_pci);
    }
}

#endif //__PLAYER_DVD_H
