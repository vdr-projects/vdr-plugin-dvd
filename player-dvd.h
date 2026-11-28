/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#ifndef VDR_DVD_PLAYER_DVD_H
#define VDR_DVD_PLAYER_DVD_H

#include "common-dvd.h"
#include "dvdspu.h"

// C++ Standard Library
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <memory>

// libdvdnav
#include <dvdnav/dvd_types.h>
#include <dvdnav/dvdnav.h>
#include <dvdnav/dvdnav_events.h>
#include <dvdread/nav_types.h>

// dvdnav.h includes version.h since libdvdnav 6.1.0. Before, there was no
// DVDNAV_VERSION_CODE, and 5.x had no DVDNAV_VERSION either (0 in #if).
#ifndef DVDNAV_VERSION_CODE
#define DVDNAV_VERSION_CODE(major, minor, micro) ((major) * 10000 + (minor) * 100 + (micro))
#endif

// VDR
#include <vdr/device.h>
#include <vdr/player.h>
#include <vdr/ringbuffer.h>
#include <vdr/spu.h>
#include <vdr/thread.h>
#include <vdr/tools.h>

class cDvdPlayer;

class IntegerListObject : public cListObject {
  private:
    int value;

  public:
    explicit IntegerListObject(int Value) : value(Value) {}

    [[nodiscard]] auto getValue() const -> int { return value; }
};

class cDvdPlayerControl;
class cIframeAssembler;
class cDVDPlayerResume;

// NOLINTNEXTLINE(misc-multiple-inheritance) -- standard VDR pattern; cf. cDvbPlayer in VDR core.
class cDvdPlayer : public cPlayer, cThread {
  private:
    enum class ePlayMode : uint8_t { Play, Pause, Slow, Fast, Still };
    enum class ePlayDir : uint8_t { Forward, Backward };
    // the bits of what playPacket() played
    static constexpr int pktNone = 0;
    static constexpr int pktVideo = 1;
    static constexpr int pktAudio = 2;

    cDvdPlayerControl *controller{nullptr};

    cSPUassembler SPUassembler;
    cSpuDecoder *SPUdecoder{nullptr};

    std::unique_ptr<cIframeAssembler> iframeAssembler;
    int IframeCnt{0};
    std::array<uint8_t, 4096> event_buf{}; // for dvdnav_get_next_cache_block(), at least a block

    std::unique_ptr<cRingBufferFrame> ringBuffer;
    cFrame *rframe{nullptr};
    cFrame *pframe{nullptr};

    dvdnav_cell_change_event_t lastCellEventInfo{};
    uint32_t pgcCurrentBlockNum{1};
    uint32_t pgcTotalBlockNum{1};
    int64_t pgcTotalTicks{1};
    int64_t pgcTicksPerBlock{1};
    auto UpdateBlockInfo() -> bool;

    bool isInFeature{false};
    int dvd_aspect{0};
    int dvd_scaleperm{0};
    void UpdateVTSInfo();

    bool skipPlayVideo{false};
    uchar videoPictureType{0}; // of the last picture header, 0 = none since Empty()
    int fastWindFactor{1};
    uint32_t cntVidBlocksPlayed{0};
    uint32_t cntAudBlocksPlayed{0};

    // Subpicture streams have a logical number, which the disc's attributes,
    // its menus and VDR's track list use, and a physical one in the VOB for
    // each display mode.
    int currentNavSubpStream{-1};   // logical, bit 7 set: forced subtitles only; -1 = none
    int currentNavSubpPhysical{-1}; // the physical stream shown for it, -1 = none
    uint32_t subpPhysicalSeen{0};   // physical streams whose packets turned up
    bool currentNavSubpStreamUsrLocked{false};

    cList<IntegerListObject> navSubpStreamSeen;
    void clearSeenSubpStream();
    void setAllSubpStreams();
    auto SearchSubpStream(int SubpStreamId) const -> int;
    void notifySeenSubpStream(int navSubpStream);
    void SelectSubpStream();
    void SetSubpStream(int id);
    void SetCurrentNavSubpStreamUsrLocked(bool lock);

    uint64_t stillTimer{0}; // in time_ticks(), 0 = no still frame
    dvdnav_t *nav{nullptr};
    pci_t *current_pci{nullptr};
    int prev_e_ptm{0};
    int ptm_offs{0};

    // The VOBUs read last, to find the one on screen: ReplayFromPicture().
    struct cVobuPosition {
        uint32_t start, end; // its pictures' PTS as the device gets them
        uint32_t block;      // position in the program chain
        int32_t title, pgcN;
    };
    static constexpr int VobuHistorySize = 32; // > 10 s, more than any device holds
    std::array<cVobuPosition, VobuHistorySize> vobuHistory{};
    int vobuHistoryCount{0};
    int vobuHistoryNext{0};
    bool replayFromPictureOnPlay{false};
    void NoteVobu(const pci_t *Pci);
    auto ReplayFromPicture() -> bool;

    uint8_t stillFrame{0};
    uint8_t lastFrameType{0};
    uint64_t VideoPts{0};
    uint64_t pktpts{0};

    int currentNavAudioTrack{-1};     // until libdvdnav announces the active stream
    int currentNavAudioTrackType{-1}; // aAC3, aDTS, aLPCM, aMPEG
    bool currentNavAudioTrackUsrLocked{false};

    cList<IntegerListObject> navAudioTracksSeen;
    void clearSeenAudioTrack();
    void setAllAudioTracks();
    void notifySeenAudioTrack(int navAudioTrack);
    auto SearchAudioStream(int AudioStreamId) const -> int;
    void SetCurrentNavAudioTrackType(int atype);

    std::atomic<bool> running{false};
    std::atomic<bool> active{true};
    std::atomic<bool> navFailed{false}; // see NavLog()
#if DVDNAV_VERSION >= DVDNAV_VERSION_CODE(6, 1, 0)
    static void NavLog(void *Player, dvdnav_logger_level_t Level, const char *Format, va_list Args)
        __attribute__((format(printf, 3, 0)));
    cMutex navLogMutex;    // libdvdnav logs from whatever thread calls it
    cStringList navLogged; // the notes NavLog() logged
    auto NavLogFirst(const char *Msg) -> bool;
#endif
    std::atomic<ePlayMode> playMode{ePlayMode::Play}; // the thread's loop checks it without the lock
    ePlayDir playDir{ePlayDir::Forward};
    int trickSpeed;

    static constexpr const char *dummy_title = "DVD Title";
    static constexpr const char *dummy_n_a = "n.a.";
    void SetTitleString();
    void SetTitleInfoString();
    void SetAspectString();

    // What the control shows, kept up to date by the player thread: the
    // control polls it from VDR's main loop, which must not wait for the
    // thread's lock while the thread reads the disc or drains the device.
    mutable cMutex infoMutex;
    bool infoPositionValid{false};
    uint32_t infoCurrentBlock{0};
    uint32_t infoTotalBlock{0};
    int64_t infoPgcLength{0};
    cString title_str{dummy_title};
    cString titleinfo_str{dummy_title};
    cString aspect_str{dummy_n_a};
    cString infoAudio{dummy_n_a};
    cString infoSubtitle{dummy_n_a};
    std::atomic<bool> menuButtons{false}; // IsInMenuDomain()
    void ReplaceInfoString(cString &Str, const char *New);
    void UpdateTrackInfo();

    // Playback
    void StillSkip();
    void ForgetButtons();
    auto PartPlay(int Title, int Part) -> bool;
    auto MenuCall(DVDMenuID_t Menu) -> int;
    auto CurrentTitle() -> int;
    void TrickSpeed(int Increment);
    void Empty(bool emptyDeviceToo = true);
    void PlayFromBlock(int64_t Block);

    // Menu buttons, still pictures and the aspect ratio
    void ClearButtonHighlight();
    void UpdateButtonHighlight(const dvdnav_highlight_event_t *hlevt);
    void DoScaleMode(int &vaspect) const;
    void SendIframe(bool doSend);

    auto playPacket(unsigned char *&cache_buf, bool trickMode, bool noAudio) -> int;
    void SetAvailableAudioTrack(eTrackType Type, int Index, uint16_t Id);
    std::array<int, ttMaxTrackTypes> announcedLanguageCode{}; // per track, -1 = not announced
    eTrackType currentAudioTrack{ttNone};                     // VDR's current audio track, as set by the player or VDR
    // An audio track VDR selects from another thread, for the player thread
    // to switch to, see SetAudioTrack():
    cMutex audioTrackRequestMutex;
    eTrackType audioTrackRequest{ttNone}; // ttNone: none
    bool audioTrackRequestByViewer{false};
    std::atomic<tThreadId> playerThreadId{0};
    void SwitchAudioTrack(eTrackType Type, bool ByViewer);
    void SwitchRequestedAudioTrack();
    void playSPU(int spuId);
    // The control shows its display, or another one than the player has the OSD.
    [[nodiscard]] auto OsdInUse() const -> bool;
    // The player takes the OSD for the subpicture, unless another one has it.
    auto TakeOsd() -> bool;

    static auto DolbyTrackIndex(int SubStreamId) -> int;
    bool subtitleTrackSync{false}; // the player announces or reports a subtitle track to VDR
    void AnnounceSubtitleTrack(int Stream);
    void ReportSubtitleTrack();
    bool subpMenuChoice{false}; // the viewer used a button of the disc's menus
    auto SubtitlesShown(int Stream) const -> bool;
    auto LogicalAudioStream(int Physical) const -> int;
    auto LogicalSubpStream(int Physical) const -> int;
    auto PhysicalSubpStream(int Logical) const -> int;
    auto GetAudioTrackLanguageCode(int channel) const -> uint16_t;
    auto AudioTrackDescription(int Channel, const char *LanguageName) const -> cString;
    auto SubtitleTrackDescription(int Channel, const char *LanguageName) const -> cString;
    auto GetAudioStreamNumbers() const -> int;
    auto GetSubtitleLanguageCode(int Channel) const -> uint16_t;
    auto GetSubtitleStreamNumbers() const -> int;

    // Resuming
    std::unique_ptr<cDVDPlayerResume> resume; // never NULL
    // The disc's key in the resume file, NULL if libdvdnav can't tell the
    // number of titles.
    [[nodiscard]] auto GetDVDResumeKey() const -> cString;
    // Stores the position in the title (not in a menu) in the resume file.
    void SaveResume();
    // The position the resume file has for the disc, 30 seconds back; false
    // if it has none.
    auto LoadResume(int &title, int &chapter, int64_t &second) -> bool;

    void Action() override;
    int emptyCount{0}; // Empty() calls, for WaitDeviceDrained(); under the thread lock

  protected:
    void Activate(bool On) override;
    void DeviceReset();
    void ClearDevice();
    auto WaitDeviceDrained() -> bool;

  public:
    // The position in the program chain and its length, in frames at
    // FramesPerSecond(): from the block numbers, as if all blocks played
    // equally long. SnapToIFrame has no effect.
    auto GetIndex(int &CurrentFrame, int &TotalFrame, bool SnapToIFrame) -> bool override;
    auto GetReplayMode(bool &Playing, bool &Forwards, int &Speed) -> bool override;
    void SetAudioTrack(eTrackType Type, const tTrackId *TrackId) override;
    void SetSubtitleTrack(eTrackType Type, const tTrackId *TrackId) override;

  protected:
    void seenSpuPts(uint64_t pts);
    static auto time_ticks() -> uint64_t;
    static void delay_ticks(uint64_t ticks);

  public:
    static constexpr int MaxAudioTracks = 0x20;
    static constexpr int AudioTrackMask = 0x1F;
    static constexpr int AC3AudioTrackMask = 0x07;
    static constexpr int MaxSubpStreams = 0x20;
    static constexpr int SubpStreamMask = 0x1F;

    cDvdPlayer();
    cDvdPlayer(const cDvdPlayer &) = delete;
    cDvdPlayer(cDvdPlayer &&) noexcept = delete;
    auto operator=(const cDvdPlayer &) -> cDvdPlayer & = delete;
    auto operator=(cDvdPlayer &&) noexcept -> cDvdPlayer & = delete;
    ~cDvdPlayer() override;
    auto Active() const -> bool { return active; }
    auto DVDActiveAndRunning() const -> bool { return active && nav != nullptr && running; }
    auto DVDRemoveable() const -> bool { return !active && !running; }

    // For the control
    void setController(cDvdPlayerControl *ctrl);
    // The thread holds its lock from checking that no other OSD is open to
    // drawing the subpicture.
    auto Thread() -> cThread * { return this; }
    auto IsDvdNavigationForced() const -> bool;
    void DrawSPU();
    void HideSPU();
    void EmptySPU();

    void Pause();
    void Play();
    void Stop();
    void Forward();
    void Backward();

    // The title, its position, the tracks and the aspect ratio for the
    // header of the replay display.
    [[nodiscard]] auto GetDisplayHeaderLine() const -> cString;

    // The length of the program chain (mostly the whole title) in PTS ticks,
    // 90000 a second.
    auto GetPGCLengthInTicks() const -> int64_t;

    // The time of a block of the program chain and the chain's length, in
    // ticks, as if all blocks played equally long.
    void BlocksToPGCTicks(uint32_t BlockNum, int64_t &Ticks, int64_t &TotalTicks) const;

    // The reverse: the block at a time and the program chain's number of
    // blocks.
    void PGCTicksToBlocks(int64_t Ticks, int64_t &BlockNum, uint32_t &TotalBlockNum) const;

    auto GetPositionInSec(int64_t &CurrentSec, int64_t &TotalSec) const -> bool;
    void GetPositionInTicks(int64_t &CurrentTicks, int64_t &TotalTicks) const;

    void SkipSeconds(int Seconds);
    // Plays the program chain from the given second on.
    void Goto(int Seconds);

    // The Goto functions return the angle, title or chapter they went to, -1
    // if none. The Next and Previous ones wrap around, from the last to the
    // first and back.
    auto GotoAngle(int Angle) -> int;
    void NextAngle();
    auto GotoTitle(int Title, int Chapter = 1) -> int;
    void NextTitle();
    void PreviousTitle();
    auto GotoPart(int Part) -> int;
    void NextPart();
    void PreviousPart();

    auto GetCurrentNavSubpStream() const -> int;
    void NextSubpStream();

    void SetCurrentNavAudioTrackUsrLocked(bool lock);
    auto GetCurrentNavAudioTrackIdx() const -> int;

    void selectUpButton();
    void selectDownButton();
    void selectLeftButton();
    void selectRightButton();
    void activateButton();

    auto callRootMenu() -> int;
    auto callTitleMenu() -> int;
    auto callSubpMenu() -> int;
    auto callAudioMenu() -> int;

    auto IsInMenuDomain() const -> bool;

    // The PTS of a packet passed on: the SPU decoder's time.
    void seenAPTS(uint64_t pts);
    void seenVPTS(uint64_t pts);

    static auto PTSTicksToSec(int64_t ticks) -> int64_t { return ticks / 90000L; }
};

// --- cDvdPlayer ---------------------------------------------------

inline auto cDvdPlayer::IsInMenuDomain() const -> bool {
    // current_pci->hli.hl_gi.hli_ss as of the last NAV packet; current_pci
    // points into nav, which the player thread closes when it ends
    return menuButtons;
}

// Moving between a menu's buttons

inline void cDvdPlayer::selectUpButton() {
    DEBUGDVD("selectUpButton\n");
    LOCK_THREAD;
    if (nav && current_pci) {
        dvdnav_upper_button_select(nav, current_pci);
    }
}

inline void cDvdPlayer::selectDownButton() {
    DEBUGDVD("selectDownButton\n");
    LOCK_THREAD;
    if (nav && current_pci) {
        dvdnav_lower_button_select(nav, current_pci);
    }
}

inline void cDvdPlayer::selectLeftButton() {
    DEBUGDVD("selectLeftButton\n");
    LOCK_THREAD;
    if (nav && current_pci) {
        dvdnav_left_button_select(nav, current_pci);
    }
}

inline void cDvdPlayer::selectRightButton() {
    DEBUGDVD("selectRightButton\n");
    LOCK_THREAD;
    if (nav && current_pci) {
        dvdnav_right_button_select(nav, current_pci);
    }
}

#endif // VDR_DVD_PLAYER_DVD_H
