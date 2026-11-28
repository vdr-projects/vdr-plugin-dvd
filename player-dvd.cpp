/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#include "player-dvd.h"
#include "common-dvd.h"
#include "control-dvd.h"
#include "dvddev.h"
#include "mpegtypes.h"
#include "setup-dvd.h"
#include "tools-dvd.h"

// C++ Standard Library
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cinttypes>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// POSIX
#include <string.h> // NOLINT(modernize-deprecated-headers) -- strdup() is POSIX, <cstring> need not have it
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>

// libdvdnav
#include <dvdnav/dvd_types.h>
#include <dvdnav/dvdnav.h>
#include <dvdnav/dvdnav_events.h>
#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/nav_types.h>

// VDR
#include <vdr/config.h>
#include <vdr/device.h>
#include <vdr/i18n.h>
#include <vdr/ringbuffer.h>
#include <vdr/skins.h>
#include <vdr/thread.h>
#include <vdr/tools.h>
#include <vdr/videodir.h>

// 100 ms each: a device that takes no data for 10 seconds in normal play.
constexpr int POLLTIMEOUTS_BEFORE_DEVICECLEAR = 100;

#ifdef CTRLDEBUG

namespace {

void printCellInfo(const dvdnav_cell_change_event_t &lastCellEventInfo, int64_t pgcTicksPerBlock) {
    const int cell_length_s = static_cast<int>(lastCellEventInfo.cell_length / 90000L);
    const int pg_length_s = static_cast<int>(lastCellEventInfo.pg_length / 90000L);
    const int pgc_length_s = static_cast<int>(lastCellEventInfo.pgc_length / 90000L);
    const int cell_start_s = static_cast<int>(lastCellEventInfo.cell_start / 90000L);
    const int pg_start_s = static_cast<int>(lastCellEventInfo.pg_start / 90000L);

    printf("cellN %ld, pgN=%d, cell_length=%ld %d, pg_length=%ld %d, pgc_length=%ld %d, cell_start %ld %d, pg_start "
           "%ld %d, ticksPerBlock=%ld, secPerBlock=%ld\n",
           static_cast<long>(lastCellEventInfo.cellN), lastCellEventInfo.pgN,
           static_cast<long>(lastCellEventInfo.cell_length), cell_length_s,
           static_cast<long>(lastCellEventInfo.pg_length), pg_length_s, static_cast<long>(lastCellEventInfo.pgc_length),
           pgc_length_s, static_cast<long>(lastCellEventInfo.cell_start), cell_start_s,
           static_cast<long>(lastCellEventInfo.pg_start), pg_start_s, static_cast<long>(pgcTicksPerBlock),
           (pgcTicksPerBlock / 90000L));
}

} // namespace

#endif

// --- cIframeAssembler ------------------------------------------------------

// Only used with the player's thread lock held.
class cIframeAssembler {
  private:
    int head{0};
    int size;
    std::vector<unsigned char> buf;

  public:
    explicit cIframeAssembler(int Size) : size(Size), buf(static_cast<size_t>(Size)) {}
    [[nodiscard]] auto Available() const -> int { return head; }
    void Clear() { head = 0; }
    auto Get(int &Length) -> unsigned char *;
    void Put(const unsigned char *Data, int Length);
};

auto cIframeAssembler::Get(int &Length) -> unsigned char * {
    Length = head;
    return buf.data();
}

void cIframeAssembler::Put(const unsigned char *Data, int Length) {
    if (head + Length < size) {
        memcpy(buf.data() + head, Data, static_cast<size_t>(Length));
        head += Length;
    } else {
        esyslog("ERROR: dvd-plugin iframeassembler full %d", head + Length);
    }
}

// --- cResumeEntry ------------------------------------------------------------

namespace {

// The resume file and its entries come from the mplayer plugin.
class cResumeEntry : public cListObject {
  public:
    std::string key;
    int title = 0;
    int chapter = 0;
    int64_t second = 0;
};

// A line of the resume file, "title:chapter:second:key"; false if it is none.
auto ParseResume(std::string_view Line, cResumeEntry &Entry) -> bool {
    const char *p = Line.data();
    const char *end = p + Line.size();
    const auto field = [&p, end](auto &Value) -> bool {
        const auto [next, error] = std::from_chars(p, end, Value);
        if (error != std::errc() || next == end || *next != ':') {
            return false;
        }
        p = next + 1;
        return true;
    };
    if (!field(Entry.title) || !field(Entry.chapter) || !field(Entry.second) || p == end) {
        return false;
    }
    Entry.key.assign(p, end);
    return true;
}

} // namespace

// --- cDVDPlayerResume ----------------------------------------------------------

// The resume file, in VDR's video directory unless RESUMEDIR names another.
constexpr const char *RESUME_FILE = "dvdplayer.resume";
#ifndef RESUMEDIR
#define RESUMEDIR cVideoDirectory::Name()
#endif

// A line per disc: "title:chapter:second:key".
class cDVDPlayerResume : public cList<cResumeEntry> {
  private:
    cString resfile;
    bool modified{false}; // the entries in memory differ from the file
    // Replaces the entries in memory with the file's.
    void LoadResume();
    // Writes the entries to the file if they changed; false if that failed.
    auto SaveResume() -> bool;
    // NULL if there is no entry for the key.
    auto FindResume(const char *key) -> cResumeEntry *;

  public:
    cDVDPlayerResume();
    cDVDPlayerResume(const cDVDPlayerResume &) = delete;
    cDVDPlayerResume(cDVDPlayerResume &&) noexcept = delete;
    auto operator=(const cDVDPlayerResume &) -> cDVDPlayerResume & = delete;
    auto operator=(cDVDPlayerResume &&) noexcept -> cDVDPlayerResume & = delete;
    ~cDVDPlayerResume() override;
    // Both read the file first, which keeps the other discs' entries.
    // Stores the position for the key and writes the file.
    void SetResume(const char *key, int title, int chapter, int64_t second);
    // The position stored for the key; false if there is none, and the
    // arguments keep their values.
    auto GetResume(const char *key, int &title, int &chapter, int64_t &second) -> bool;
};

cDVDPlayerResume::cDVDPlayerResume() : resfile(AddDirectory(RESUMEDIR, RESUME_FILE)) {}

cDVDPlayerResume::~cDVDPlayerResume() { SaveResume(); }

void cDVDPlayerResume::SetResume(const char *key, int title, int chapter, int64_t second) {
    LoadResume();
    cResumeEntry *re = FindResume(key);
    if (re) {
        DEBUGDVD("resume: setting resume %d:%d:%lld (update)", title, chapter, static_cast<long long>(second));
    } else {
        re = new cResumeEntry;
        re->key = key;
        Add(re);
        DEBUGDVD("resume: setting resume %d:%d:%lld (new)", title, chapter, static_cast<long long>(second));
    }
    re->title = title;
    re->chapter = chapter;
    re->second = second;
    modified = true;
    SaveResume();
}

auto cDVDPlayerResume::GetResume(const char *key, int &title, int &chapter, int64_t &second) -> bool {
    LoadResume();
    const cResumeEntry *re = FindResume(key);
    if (re) {
        title = re->title;
        chapter = re->chapter;
        second = re->second;
        return true;
    }
    return false;
}

void cDVDPlayerResume::LoadResume() {
    // Without a file, no entries.
    Clear();
    modified = false;
    DEBUGDVD("resume: resume file is \"%s\"\n", *resfile);
    std::ifstream file(*resfile);
    std::string line;
    while (std::getline(file, line)) {
        auto *re = new cResumeEntry;
        if (ParseResume(line, *re)) {
            Add(re);
        } else {
            delete re;
        }
    }
}

auto cDVDPlayerResume::SaveResume() -> bool {
    if (modified) {
        DEBUGDVD("resume: saving resume file\n");
        cSafeFile f(resfile);
        if (f.Open()) {
            for (const cResumeEntry *re = First(); re; re = Next(re)) {
                // The error stays in the stream: Close() keeps the old file.
                if (fprintf(f, "%d:%d:%" PRId64 ":%s\n", re->title, re->chapter, re->second, re->key.c_str()) < 0) {
                    break;
                }
            }
            return f.Close();
        }
        DEBUGDVD("resume: failed to save resume file\n");
        return false;
    }
    return true;
}

auto cDVDPlayerResume::FindResume(const char *key) -> cResumeEntry * {
    DEBUGDVD("resume: searching resume position for \"%s\"\n", key);
    for (cResumeEntry *re = First(); re; re = Next(re)) {
        if (!strcasecmp(re->key.c_str(), key)) {
            DEBUGDVD("resume: found resume position %d:%d:%lld\n", re->title, re->chapter,
                     static_cast<long long>(re->second));
            return re;
        }
    }
    DEBUGDVD("resume: no resume position found\n");
    return nullptr;
}

// --- cDvdPlayer ------------------------------------------------------------

// The two letters of a language code of libdvdnav's, the first in the low
// byte as the player keeps it, and a terminating 0.
namespace {

using LanguageLetters = std::array<char, 3>;
auto Letters(uint16_t LanguageCode) -> LanguageLetters {
    return {static_cast<char>(LanguageCode & 0xFF), static_cast<char>(LanguageCode >> 8), 0};
}

// The ISO 639-1 code a DVD gives for a track's language in Language, empty
// if there is none, and the language's name, or NULL.
auto TrackLanguage(uint16_t LanguageCode, LanguageLetters &Language) -> const char * {
    Language = Letters(LanguageCode);
    if (LanguageCode == 0xFFFF || !isalpha(static_cast<unsigned char>(Language.at(0))) ||
        !isalpha(static_cast<unsigned char>(Language.at(1)))) {
        Language.at(0) = 0;
        return nullptr;
    }
    for (size_t i = 0; i < ISO639code.size(); i++) {
        if (!strcasecmp(Language.data(), ISO639code.at(i))) {
            return DvdLanguageName.at(i);
        }
    }
    return nullptr;
}

} // namespace

// The ring buffer of the packets for the device.
constexpr int VIDEOBUFSIZE = MEGABYTE(1);

// The buffer of the I-frame of a still picture.
constexpr int CIF_MAXSIZE = 512; // KB

// The speeds as in VDR's player; beyond MAX_SPEEDS, the player skips blocks.
constexpr int MAX_VIDEO_SLOWMOTION = 63; // the largest Speed for DeviceTrickSpeed(), as VDR's player passes it
constexpr int NORMAL_SPEED = 10;         // the index of the '1' entry in the following array
constexpr int MAX_SPEEDS = 3;            // the offset of the maximum speed from normal speed in either direction
constexpr int MAX_MAX_SPEEDS = MAX_SPEEDS * MAX_SPEEDS; // the offset of the maximum speed skipping blocks
constexpr int SPEED_MULT = 12;                          // the speed multiplier
constexpr std::array<int, 21> Speeds{0, 0, 0, 0, 0, 0, 0, -2, -4, -8, 1, 2, 4, 12, 0, 0, 0, 0, 0, 0, 0};

cDvdPlayer::cDvdPlayer()
    : cThread("dvd-plugin"), iframeAssembler(std::make_unique<cIframeAssembler>(KILOBYTE(CIF_MAXSIZE))),
      ringBuffer(std::make_unique<cRingBufferFrame>(VIDEOBUFSIZE)), trickSpeed(NORMAL_SPEED),
      resume(std::make_unique<cDVDPlayerResume>()) {
    DEBUGDVD("cDvdPlayer::cDvdPlayer(void)\n");
    DEBUG_SUBP_ID("SPU showSubs=%d\n", DVDSetup.ShowSubtitles);
    clearSeenSubpStream();
    clearSeenAudioTrack();
}

cDvdPlayer::~cDvdPlayer() {
    DEBUGDVD("destructor cDvdPlayer::~cDvdPlayer()\n");
    Detach();
}

void cDvdPlayer::setController(cDvdPlayerControl *ctrl) { controller = ctrl; }

auto cDvdPlayer::IsDvdNavigationForced() const -> bool {
    return (controller ? controller->IsDvdNavigationForced() : false);
}

auto cDvdPlayer::OsdInUse() const -> bool { return controller ? controller->OsdVisible(this) : false; }

auto cDvdPlayer::TakeOsd() -> bool { return controller ? controller->TakeOsd(this) : false; }

void cDvdPlayer::TrickSpeed(int Increment) {
    const int nts = trickSpeed + Increment;
    cntVidBlocksPlayed = 0;
    cntAudBlocksPlayed = 0;

    // 0 beyond MAX_SPEEDS
    const int speed = nts > 0 && nts - NORMAL_SPEED <= MAX_SPEEDS ? Speeds.at(static_cast<size_t>(nts)) : 0;
    if (speed == 1) {
        fastWindFactor = 1;
        trickSpeed = nts;

        if (playMode == ePlayMode::Fast) {
            Play();
        } else {
            Pause();
        }
    } else if (speed != 0) {
        fastWindFactor = 1;
        if (playDir == ePlayDir::Backward && DVDSetup.ReadAHead > 0) {
            dvdnav_set_readahead_flag(nav, 0);
        }
        trickSpeed = nts;
        const int Mult = (playMode == ePlayMode::Slow) ? 1 : SPEED_MULT;
        int sp = std::min((speed > 0) ? Mult / speed : -speed * Mult, MAX_VIDEO_SLOWMOTION);

        if (playDir == ePlayDir::Backward) {
            fastWindFactor = (playMode == ePlayMode::Slow) ? trickSpeed - NORMAL_SPEED : trickSpeed - NORMAL_SPEED + 1;
            if (playMode == ePlayMode::Slow) {
                sp = 2;
            }
        }
        DeviceTrickSpeed(sp, playDir == ePlayDir::Forward);
    } else if (nts > 0 && nts - NORMAL_SPEED <= MAX_MAX_SPEEDS) {
        fastWindFactor = 1;
        trickSpeed = nts;
        if (playDir == ePlayDir::Backward) {
            if (playMode == ePlayMode::Slow) {
                fastWindFactor = trickSpeed - NORMAL_SPEED;
            } else {
                fastWindFactor = (trickSpeed - NORMAL_SPEED - MAX_SPEEDS + 1) * 2;
            }
        } else if (playDir == ePlayDir::Forward && playMode == ePlayMode::Fast) {
            fastWindFactor = (trickSpeed - NORMAL_SPEED - MAX_SPEEDS + 1) * 2;
        }
    }

    DEBUG_CONTROL("dvd TRICKSPEED: %d (%d), fastWindFactor=%d fwd=%d, slow=%d, fast=%d\n", trickSpeed, Increment,
                  fastWindFactor, playDir == ePlayDir::Forward, playMode == ePlayMode::Slow,
                  playMode == ePlayMode::Fast);
}

void cDvdPlayer::DeviceReset() {
    DEBUG_CONTROL("DeviceReset: Clear & Play\n");
    cntVidBlocksPlayed = 0;
    cntAudBlocksPlayed = 0;
    cPlayer::DeviceClear();
    cPlayer::DevicePlay();
}

// Clears what VDR holds of a PES packet too, then the device.
void cDvdPlayer::ClearDevice() {
    DEBUG_CONTROL("ClearDevice\n");
    if (IsAttached()) {
        PlayPes(nullptr, 0);
    }
    DeviceClear();
}

// How often the player thread refreshes the track info the control shows.
constexpr int TRACKINFOINTERVALMS = 200;

// Consecutive read errors after which the player gives up.
constexpr int MAXREADERRORS = 5;

#if APIVERSNUM > 30013
// How long the picture may stand still in normal play before
// WaitDeviceDrained() stops waiting: the device holds no more than that
// picture then, or it can't tell that it has played out - VDR's default
// DrainDevice() never does.
constexpr uint64_t DRAINSTALLMS = 1000;
#else
// How long WaitDeviceDrained() waits at most for the device to play out.
constexpr int DRAINTIMEOUTMS = 6000;
#endif

auto cDvdPlayer::WaitDeviceDrained() -> bool {
    // Called with the thread lock held, which is released while the device
    // plays out: that takes as long as the device buffers, with some output
    // devices up to nine seconds of a clip without audio, and the control's
    // keys must not wait for it. Returns false if one of them emptied the player
    // or stopped it meanwhile: what the caller held is gone.
    // A fixed limit cut such a clip short.
#if APIVERSNUM > 30013
    const int emptied = emptyCount;
    uint64_t stc = DeviceGetSTC();
    cTimeMs stcMoved;
    while (!DeviceDrain()) {
        const uint64_t now = DeviceGetSTC();
        // A frozen device plays nothing out until it runs again.
        if (now != stc || playMode != ePlayMode::Play) {
            stc = now;
            stcMoved.Set();
        } else if (stcMoved.Elapsed() > DRAINSTALLMS) {
            dsyslog("dvd-plugin: device not drained, its picture stands still");
            break;
        }
        Unlock();
        cCondWait::SleepMs(10);
        Lock();
        // cppcheck-suppress knownConditionTrueFalse ; Empty() ran while unlocked
        if (emptyCount != emptied || !running) {
            return false;
        }
    }
#else
    cTimeMs Timeout(DRAINTIMEOUTMS);
    while (!DeviceFlush(100) && !Timeout.TimedOut())
        ;
#endif
    return true;
}

void cDvdPlayer::DrawSPU() {
    LOCK_THREAD;
    if (SPUdecoder && !SPUdecoder->IsVisible() && TakeOsd()) {
        SPUdecoder->Draw();
    }
}

void cDvdPlayer::HideSPU() {
    LOCK_THREAD;
    if (SPUdecoder) {
        SPUdecoder->Hide();
    }
}

void cDvdPlayer::EmptySPU() {
    if (SPUdecoder) {
        ClearButtonHighlight();
        SPUdecoder->Empty();
    }
}

void cDvdPlayer::Empty(bool emptyDeviceToo) {
    DEBUG_CONTROL("dvd .. Empty ...\n");

    LOCK_THREAD;

    if (IframeCnt < 0 && cntVidBlocksPlayed > 0) {
        DEBUG_IFRAME("I-Frame: Empty: Iframe used, VidBlocksPlayed -> emptyDevice\n");
        emptyDeviceToo = true;
    }

    DEBUG_CONTROL("dvd .. Empty IframeCnt=%d, emptyDeviceToo=%d!\n", IframeCnt, emptyDeviceToo);

    if (SPUdecoder) {
        DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
        EmptySPU();
    }
    lastFrameType = 0xff;
    VideoPts = 0xFFFFFFFF;
    pktpts = 0xFFFFFFFF;
    emptyCount++;
    prev_e_ptm = 0;
    ptm_offs = 0;
    vobuHistoryCount = 0; // its PTS had the old offset
    replayFromPictureOnPlay = false;

    DELETENULL(rframe);
    pframe = nullptr;
    ringBuffer->Clear();
    SPUassembler.Reset();
    cntVidBlocksPlayed = 0;
    cntAudBlocksPlayed = 0;

    stillTimer = 0;
    stillFrame = 0;
    IframeCnt = 0;
    iframeAssembler->Clear();

    skipPlayVideo = false;
    videoPictureType = 0;

    if (emptyDeviceToo) {
        ClearDevice();
    }
}

void cDvdPlayer::StillSkip() {
    DEBUGDVD("cDvdPlayer::StillSkip()\n");
    if (stillTimer != 0) {
        stillTimer = 0;
        if (nav != nullptr) {
            dvdnav_still_skip(nav);
        }
    }
}

// After a jump of the player's own, libdvdnav keeps the PCI of the last NAV
// packet until it reads the first one of the jump's target. A button of the
// menu left, activated meanwhile, ran in the target's domain: its JumpTT
// from a title failed an assertion in libdvdnav's VM and aborted VDR.
void cDvdPlayer::ForgetButtons() {
    current_pci = nullptr;
    menuButtons = false;
}

// Unlike libdvdnav's other jumps, dvdnav_part_play() keeps the still of the
// cell it leaves: out of a still menu, libdvdnav reported that still again
// and again and the title never started. dvdnav_still_skip() would skip the
// still at the end of the target's first cell instead, the jump to the top
// of the program dvdnav_part_play() went to clears it.
auto cDvdPlayer::PartPlay(int Title, int Part) -> bool {
    if (dvdnav_part_play(nav, Title, Part) != DVDNAV_STATUS_OK) {
        return false;
    }
    dvdnav_top_pg_search(nav);
    ForgetButtons();
    return true;
}

// dvdnav_menu_call() leaves a still itself. dvdnav_still_skip() before it
// skipped the still of the menu called, which a still menu ends with.
auto cDvdPlayer::MenuCall(DVDMenuID_t Menu) -> int {
    if (!nav || dvdnav_menu_call(nav, Menu) != DVDNAV_STATUS_OK) {
        return 0;
    }
    ForgetButtons();
    return 1;
}

#ifdef DVDDEBUG
namespace {

// The name of a libdvdnav event; it no longer sends 11, SEEK_DONE.
auto EventName(int32_t Event) -> const char * {
    static constexpr std::array<const char *, 15> Names{{"BLOCK_OK", "NOP", "STILL_FRAME", "SPU_STREAM_CHANGE",
                                                         "AUDIO_STREAM_CHANGE", "VTS_CHANGE", "CELL_CHANGE",
                                                         "NAV_PACKET", "STOP", "HIGHLIGHT", "SPU_CLUT_CHANGE",
                                                         "SEEK_DONE", "HOP_CHANNEL", "WAIT", "TRICK_MODE_CHANGE"}};
    return Event >= 0 && static_cast<size_t>(Event) < Names.size() ? Names.at(static_cast<size_t>(Event)) : "?";
}

} // namespace
#endif

// stillFrame: the cell ends in a still picture, the VOBU is one.
constexpr int CELL_STILL = 0x02;
constexpr int NAV_STILL = 0x04;

// Time in ticks of the 90 kHz MPEG clock: 90 a millisecond.
auto cDvdPlayer::time_ticks() -> uint64_t {
    // monotonic, so that setting the clock can't end or prolong a still frame
    return cTimeMs::Now() * 90U;
}

void cDvdPlayer::delay_ticks(uint64_t ticks) {
    const uint64_t t0 = time_ticks();
    usleep(1000); // sleeps at least once
    uint64_t done = time_ticks() - t0;
    while (ticks > done) {
        // usleep() oversleeps by up to about 19 ms
        if (ticks - done > uint64_t{19} * 90) {
            usleep(static_cast<useconds_t>(((ticks - done) * 100U / 9U) - 19000U));
        }
        done = time_ticks() - t0;
    }
}

auto cDvdPlayer::GetDVDResumeKey() const -> cString {
    // "DVDName_TotalTitles_OverallChapters". Not unique, but libdvdnav tells
    // neither the disc's serial number nor its size.
    int totalTitles = 0;
    if (dvdnav_get_number_of_titles(nav, &totalTitles) != DVDNAV_STATUS_OK) {
        return {};
    }
    int totalChapters = 0;
    for (int t = 1; t <= totalTitles; t++) {
        int curChapters = 0;
        dvdnav_get_number_of_parts(nav, t, &curChapters);
        totalChapters += curChapters;
        DEBUGDVD("resume: cDvdPlayer::Action() Title %d has %d chapters.\n", t, curChapters);
    }
    const cMutexLock MutexLock(&infoMutex);
    DEBUGDVD("resume: cDvdPlayer::Action() Titles: %d with %d chapters all together, Title: \"%s\"\n", totalTitles,
             totalChapters, *title_str);
    return cString::sprintf("%s_%d_%d", *title_str, totalTitles, totalChapters);
}

void cDvdPlayer::SaveResume() {
    int currentTitle = 0;
    int currentChapter = 0;
    // A menu is title 0, which can't be resumed.
    if (dvdnav_current_title_info(nav, &currentTitle, &currentChapter) != DVDNAV_STATUS_OK || currentTitle == 0) {
        DEBUGDVD("resume: ERROR fetching current title and chapter (maybe in menus?).\n");
        return;
    }
    const cString key = GetDVDResumeKey();
    if (!*key) {
        DEBUGDVD("resume: ERROR computing resume key for this dvd!\n");
        return;
    }
    int64_t currentSec = 0;
    int64_t totalSec = 0;
    GetPositionInSec(currentSec, totalSec);
    DEBUGDVD("resume->SetResume(\"%s\", %d, %d, %lld)\n", *key, currentTitle, currentChapter,
             static_cast<long long>(currentSec));
    resume->SetResume(key, currentTitle, currentChapter, currentSec);
}

auto cDvdPlayer::LoadResume(int &title, int &chapter, int64_t &second) -> bool {
    const cString key = GetDVDResumeKey();
    if (!*key) {
        DEBUGDVD("resume: ERROR computing resume key for this dvd.\n");
        return false;
    }
    DEBUGDVD("resume->GetResume(\"%s\", ...): ", *key);
    const bool found = resume->GetResume(key, title, chapter, second);
    if (!found) {
        DEBUGDVD("<none>\n");
        return false;
    }
    DEBUGDVD("%d:%d:%lld\n", title, chapter, static_cast<long long>(second));
    // Some seconds back, as VDR resumes a recording.
    const int ResumeRewind = 30;
    if (second > ResumeRewind) {
        second -= ResumeRewind;
    }
    return true;
}

void cDvdPlayer::Action() {
    playerThreadId = cThread::ThreadId();
    event_buf.fill(0);

    const unsigned char *write_blk = nullptr;
    int blk_size = 0;

    // libdvdnav writes the handle while it opens the disc, which takes
    // seconds; the control's thread reads nav under the thread lock.
    dvdnav_t *opened = nullptr;
#if DVDNAV_VERSION >= DVDNAV_VERSION_CODE(6, 1, 0)
    static const dvdnav_logger_cb navLogger = {NavLog};
    if (dvdnav_open2(&opened, this, &navLogger, cDVD::DeviceName()) != DVDNAV_STATUS_OK) {
#else
    if (dvdnav_open(&opened, cDVD::DeviceName()) != DVDNAV_STATUS_OK) {
#endif
        Skins.QueueMessage(mtError, tr("Error.DVD$Error opening DVD!"));
        esyslog("ERROR: dvd-plugin cannot open dvdnav device %s -> input thread ended (pid=%d) !", cDVD::DeviceName(),
                getpid());
        active = running = false;
        return;
    }
    {
        LOCK_THREAD;
        nav = opened;
    }
    dvdnav_set_readahead_flag(nav, DVDSetup.ReadAHead);
    if (DVDSetup.PlayerRCE != 0) {
        dvdnav_set_region_mask(nav, 1 << (DVDSetup.PlayerRCE - 1));
    } else {
        dvdnav_set_region_mask(nav, 0xffff);
    }
    // NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast) -- C-API: libdvdnav takes char *, it only reads the codes
    dvdnav_menu_language_select(nav, const_cast<char *>(LanguageCode(DVDSetup.MenuLanguage)));
    dvdnav_audio_language_select(nav, const_cast<char *>(LanguageCode(DVDSetup.AudioLanguage)));
    dvdnav_spu_language_select(nav, const_cast<char *>(LanguageCode(DVDSetup.SpuLanguage)));
    // NOLINTEND(cppcoreguidelines-pro-type-const-cast)
    DEBUGDVD("Default-Langs: menu=%s, audio=%s, spu=%s\n", LanguageCode(DVDSetup.MenuLanguage),
             LanguageCode(DVDSetup.AudioLanguage), LanguageCode(DVDSetup.SpuLanguage));
    if (IsAttached()) {
        LOCK_THREAD; // HideSPU() and DrawSPU() use it from the control's thread
        SPUdecoder = cDevice::PrimaryDevice()->GetSpuDecoder();
        DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
        EmptySPU();
    }

    int slomoloop = 0;
    uint64_t sleept = 0; // in ticks
    bool trickMode = false;
    bool noAudio = false;
    int PollTimeouts = 0;
    int ReadErrors = 0;
    int playedPacket = pktNone;

    Empty(false);

    running = true;
    eFrameType frameType = ftUnknown;

    // Reset the device once at the first audio pack (a fix for the A/V sync
    // of DVB cards with a decoder), but only at the start of the replay: after
    // an opening clip without sound, the reset cut the next clip's first GOP.
    bool firstClear = true;

    // Resuming: at the first title set change to the title and chapter, at
    // the next cell change to the second.
    bool first_vts_change = true;
    bool next_cell_change = false;
    int64_t resSecond = 0;

    cTimeMs trackInfoTimeout;

    // Running() is false once Activate(false) has called Cancel(), even if
    // it came while dvdnav_open() was still spinning up the disc.
    while (Running() && running && nav) {

        if (sleept) {
            if (sleept / 90U > 1000) {
                DEBUG_PTS("\n***** WARNING >=1000ms sleep %llums\n", static_cast<unsigned long long>(sleept / 90U));
            }
            delay_ticks(sleept);

            DEBUG_PTS2(
                "dvd loop sleep=%5ut(%3ums), blk_size=%3d, skipPlayV=%d, AudioBlock=%d IframeCnt=%d stillTimer=%llu\n",
                static_cast<unsigned int>(sleept), static_cast<unsigned int>(sleept) / 90U, blk_size, skipPlayVideo,
                playedPacket == pktAudio, IframeCnt, static_cast<unsigned long long>(stillTimer) / 90U);
        }
        sleept = 0;
        SwitchRequestedAudioTrack(); // also while paused
        if (playMode == ePlayMode::Pause || playMode == ePlayMode::Still) {
            cCondWait::SleepMs(10);
            continue;
        }

        cPoller Poller;
        if (!DevicePoll(Poller, 100)) {
            // Only in normal play: in slow motion a device takes data at a
            // fraction of the normal rate, and after a jump or a trick mode
            // it may not take any while it plays out what it holds.
            if (playMode == ePlayMode::Play && ++PollTimeouts == POLLTIMEOUTS_BEFORE_DEVICECLEAR) {
                dsyslog("dvd-plugin: clearing device because of consecutive poll timeouts %d",
                        POLLTIMEOUTS_BEFORE_DEVICECLEAR);
                DEBUG_CONTROL("clearing device because of consecutive poll timeouts %d\n",
                              POLLTIMEOUTS_BEFORE_DEVICECLEAR);
                {
                    LOCK_THREAD;
                    DeviceReset();
                }
                PollTimeouts = 0;
            }
            continue;
        }
        PollTimeouts = 0;

        LOCK_THREAD;
        if (!running) {
            continue; // a key handler found libdvdnav broken, see NavLog()
        }

        // Under the lock: Empty(), called by the control's key handlers,
        // clears the ring buffer and so deletes the frame taken from it.
        if (!pframe) {
            pframe = ringBuffer->Get();
            if (pframe) {
                write_blk = pframe->Data();
                blk_size = pframe->Count();
                frameType = pframe->Type();
            }
        }

        trickMode = playMode == ePlayMode::Fast || (playMode == ePlayMode::Slow && playDir == ePlayDir::Backward);

        if (pframe) {
            int res = blk_size;
            if (!skipPlayVideo) {
                if (firstClear && (frameType == ftDolby || frameType == ftAudio) && IframeCnt == 0) {
                    DeviceReset();
                    firstClear = false;
                }
                if (IframeCnt < 0 && frameType == ftVideo) {
                    // After a still picture (DeviceStillPicture()), video needs
                    // a reset of the device.
                    DEBUG_CONTROL("clearing device because of IframeCnt < 0 && VideoFrame\n");
                    IframeCnt = 0;
                    if (!WaitDeviceDrained()) {
                        continue; // pframe is gone
                    }
                    if (!firstClear) {
                        DeviceReset();
                    }
                }

                // The video of a still picture goes to the device as its
                // assembled I-frame, below.
                res = (IframeCnt > 0 && frameType == ftVideo) ? blk_size : PlayPes(write_blk, blk_size);

                if (trickMode) {
                    DEBUG_CONTROL("PLAYED  : size=%d, written=%d\n", blk_size, res);
                }
            }
#ifdef CTRLDEBUG
            else if (trickMode) {
                printf("SKIPPED : size=%d\n", blk_size);
            }
#endif

            if (res < 0) {
                if (errno != EAGAIN && errno != EINTR) {
                    esyslog("ERROR: dvd-plugin PlayVideo, %s (workaround activ)", strerror(errno));
                    DEBUG_CONTROL("PlayVideo, %s (workaround activ)\n", strerror(errno));
                }
                DEBUG_CONTROL("PLAYED zero -> Clear/Play\n");
                DeviceReset();
                continue;
            }
            if (res > 0) {
                blk_size -= res;
                write_blk += res;
            }

            if (blk_size <= 0) {
                if (frameType == ftVideo) {
                    if (!skipPlayVideo) {
                        cntVidBlocksPlayed++;
                    }
                } else if (frameType == ftAudio || frameType == ftDolby) {
                    cntAudBlocksPlayed++;
                }

                playedPacket = pktNone;
                frameType = ftUnknown;
                ringBuffer->Drop(pframe);
                pframe = nullptr;
            }
            continue;
        }
        if (playedPacket == pktAudio) {
            cntAudBlocksPlayed++;
        }
        playedPacket = pktNone;
        frameType = ftUnknown;

        if (IframeCnt > 0) {
            // Only if the device played video: in a menu with audio only the
            // reset would cut the sound.
            if (cntVidBlocksPlayed > 0) {
                DEBUG_CONTROL("clearing device because of IframeCnt > 0, vid %d, aud %d\n", cntVidBlocksPlayed,
                              cntAudBlocksPlayed);
                if (!WaitDeviceDrained()) {
                    continue; // the I-frame is gone
                }
                DeviceReset();
            }
            int iframeSize = 0;
            const unsigned char *iframe = iframeAssembler->Get(iframeSize);

            DEBUG_IFRAME("I-Frame: DeviceStillPicture: IframeCnt=%d->-1, iframe=%d, used=%d;\n", IframeCnt,
                         iframe != nullptr, iframeSize);

            if (iframe && iframeSize > 0) {
#ifdef IFRAMEWRITE
                static int ifnum = 0;
                const cString ifname = cString::sprintf("/tmp/dvd.iframe.%3.3d.asm.pes", ifnum++);
                if (FILE *f = fopen(ifname, "wb")) {
                    const bool written =
                        fwrite(iframe, 1, static_cast<size_t>(iframeSize), f) == static_cast<size_t>(iframeSize);
                    if (fclose(f) != 0 || !written) {
                        esyslog("ERROR: dvd-plugin can't write %s", *ifname);
                    }
                }
#endif
                DeviceStillPicture(iframe, iframeSize);

                DEBUG_IFRAME("SEND; ");
                if (!WaitDeviceDrained()) {
                    continue; // emptied, which also ended the still
                }
                DEBUG_IFRAME("FLUSH!\n");
            }
            iframeAssembler->Clear();
            IframeCnt = -1; // a still picture was played
            if (blk_size <= 0 && !skipPlayVideo) {
                sleept = uint64_t{90}; // 1 ms
            }

            DEBUG_IFRAME("I-Frame: DeviceStillPicture: vpts=%8ums sleept=%llums\n",
                         static_cast<unsigned int>(VideoPts / 90U), static_cast<unsigned long long>(sleept / 90U));
            continue;
        }

        // Fast forward beyond the device's speeds: skip ahead.
        if (playDir == ePlayDir::Forward && playMode == ePlayMode::Fast && skipPlayVideo && cntVidBlocksPlayed > 0 &&
            fastWindFactor > 1) {
            uint32_t pos = 0;
            uint32_t len = 0;
            int64_t pgcPosTicks = 0;
            int64_t pgcPosTicksIncr = 0;

            // Without a position (libdvdnav has no program chain yet) the
            // skip went to wherever the unset block number pointed.
            dvdnav_set_PGC_positioning_flag(nav, 1);
            const bool posOk = dvdnav_get_position(nav, &pos, &len) == DVDNAV_STATUS_OK;

            pgcPosTicks = static_cast<int64_t>(pos) * pgcTicksPerBlock;

            // fastWindFactor half seconds ahead, not past the end
            pgcPosTicksIncr = static_cast<int64_t>((fastWindFactor * 90000L) / 2L);

            // pgcTicksPerBlock is 0 if the program chain's playback time is
            // shorter than its number of blocks, or 0 itself.
            if (posOk && pgcPosTicks + pgcPosTicksIncr < pgcTotalTicks && pgcTicksPerBlock > 0) {
                DEBUG_CONTROL("dvd %d %4.4u fwd get block: %4.4ldb %10.10ldt %lds\n", playDir == ePlayDir::Backward,
                              cntVidBlocksPlayed, static_cast<long>(pos), static_cast<long>(pgcPosTicks),
                              (pgcPosTicks / 90000L));

                DEBUG_CONTROL("dvd fwd set block: ts=%d, factor=%ds, %ldms+%ldms=%ldms, %4.4ldb+%4.4ldb=%4.4ldb, "
                              "%10.10ldt+%10.10ldt=%10.10ldt\n",
                              trickSpeed, fastWindFactor / 2, (pgcPosTicks / 90L), (pgcPosTicksIncr / 90L),
                              ((pgcPosTicks + pgcPosTicksIncr) / 90L), static_cast<long>(pos),
                              static_cast<long>(pgcPosTicksIncr / pgcTicksPerBlock),
                              static_cast<long>((pgcPosTicks + pgcPosTicksIncr) / pgcTicksPerBlock),
                              static_cast<long>(pgcPosTicks), static_cast<long>(pgcPosTicksIncr),
                              static_cast<long>(pgcPosTicks) + static_cast<long>(pgcPosTicksIncr));

                pos = static_cast<uint32_t>((pgcPosTicks + pgcPosTicksIncr) / pgcTicksPerBlock);

                if (dvdnav_sector_search(nav, pos, SEEK_SET) != DVDNAV_STATUS_OK) {
                    esyslog("ERROR: dvd-plugin dvd error dvdnav_sector_search: %s", dvdnav_err_to_string(nav));
                }
            }
            cntVidBlocksPlayed = 0;
        }

        // Backward, fast or slow: jump back.
        else if (playDir == ePlayDir::Backward && skipPlayVideo && cntVidBlocksPlayed > 0 && fastWindFactor != 1) {
            uint32_t pos = 0;
            uint32_t posdiff = 0;
            uint32_t len = 0;

            if (fastWindFactor < 0) {
                // slow motion
                if (slomoloop == 0) {
                    slomoloop = abs(fastWindFactor);
                    cntVidBlocksPlayed++; // one more block back ..
                    posdiff = (cntVidBlocksPlayed * 2);
                    DEBUG_CONTROL("dvd slomo jump: d%ldb %dsl (%df)\n", static_cast<long>(posdiff), slomoloop,
                                  fastWindFactor);
                } else {
                    posdiff = cntVidBlocksPlayed;
                    DEBUG_CONTROL("dvd slomo loop: d%ldb %dsl (%df)\n", static_cast<long>(posdiff), slomoloop,
                                  fastWindFactor);
                    slomoloop--;
                }
            } else {
                // Up to MAX_SPEEDS back by the blocks played times
                // fastWindFactor, beyond by fastWindFactor half seconds.
                if (trickSpeed - NORMAL_SPEED <= MAX_SPEEDS || pgcTicksPerBlock <= 0) {
                    cntVidBlocksPlayed++; // one more block back ..
                    posdiff = cntVidBlocksPlayed * static_cast<uint32_t>(fastWindFactor);
                } else {
                    int64_t pgcPosTicksDecr = 0;
                    pgcPosTicksDecr = static_cast<int64_t>((fastWindFactor * 90000L) / 2L);
                    posdiff = static_cast<uint32_t>(pgcPosTicksDecr / pgcTicksPerBlock);
                }
            }

            // unless that is before the beginning
            dvdnav_set_PGC_positioning_flag(nav, 1);
            if (dvdnav_get_position(nav, &pos, &len) == DVDNAV_STATUS_OK && pos > posdiff) {
                const uint32_t forcedBlockPosition = pos - posdiff;
                DEBUG_CONTROL("dvd bwd set block: ts=%d, factor=%d, %ld-%ld=%ld, slo=%d\n", trickSpeed, fastWindFactor,
                              static_cast<long>(pos), static_cast<long>(posdiff),
                              static_cast<long>(forcedBlockPosition), slomoloop);
                if (dvdnav_sector_search(nav, forcedBlockPosition, SEEK_SET) != DVDNAV_STATUS_OK) {
                    esyslog("ERROR: dvd-plugin dvd error dvdnav_sector_search: %s", dvdnav_err_to_string(nav));
                }
            } else {
                DEBUG_CONTROL("dvd %d %4.4u bwd get block: %4.4ldb d%4.4ldb, slo=%d\n", playDir == ePlayDir::Backward,
                              cntVidBlocksPlayed, static_cast<long>(pos), static_cast<long>(posdiff), slomoloop);
            }
            cntVidBlocksPlayed = 0;
        } else if (playMode == ePlayMode::Fast || playMode == ePlayMode::Slow) {
            uint32_t pos = 0;
            uint32_t len = 0;
            int64_t pgcPosTicks = 0;

            dvdnav_set_PGC_positioning_flag(nav, 1);
            const bool posOk = dvdnav_get_position(nav, &pos, &len) == DVDNAV_STATUS_OK;

            pgcPosTicks = static_cast<int64_t>(pos) * pgcTicksPerBlock;

            DEBUG_CONTROL("dvd %d %4.4u any get block: %4.4ldb %10.10ldt %lds, sec=%d\n", playDir == ePlayDir::Backward,
                          cntVidBlocksPlayed, static_cast<long>(pos), static_cast<long>(pgcPosTicks),
                          (pgcPosTicks / 90000L), static_cast<int>(pgcPosTicks / 90000L));

            // At the last block go back 10 s instead of running past the end.
            if (posOk && playDir == ePlayDir::Forward && pos + 1 == len && pgcPosTicks > 90000L * 10L &&
                pgcTicksPerBlock > 0) {
                pgcPosTicks -= 90000L * 10L;
                if (dvdnav_sector_search(nav, pgcPosTicks / pgcTicksPerBlock, SEEK_SET) != DVDNAV_STATUS_OK) {
                    esyslog("ERROR: dvd-plugin dvd error dvdnav_sector_search: %s", dvdnav_err_to_string(nav));
                }
            }
        }

        uint8_t *cache_ptr = event_buf.data();
        int32_t event = 0;
        int32_t len = 0;

        // From here on no continue: it would bypass dvdnav_free_cache_block().

        const dvdnav_status_t navStatus = dvdnav_get_next_cache_block(nav, &cache_ptr, &event, &len);
        if (navFailed) {
            // the event comes from a VM that can't be asked anything anymore
            if (navStatus == DVDNAV_STATUS_OK && cache_ptr != nullptr && cache_ptr != event_buf.data()) {
                dvdnav_free_cache_block(nav, cache_ptr);
            }
            break;
        }
        if (navStatus != DVDNAV_STATUS_OK) {
            esyslog("ERROR: dvd-plugin %s", dvdnav_err_to_string(nav));
            if (ReadErrors++ == 0) {
                Skins.QueueMessage(mtError, tr("Error.DVD$Error fetching data from DVD!"));
            }
            // The reset starts over at the beginning of the disc. If even
            // that can't be read (the disc has been taken out), stop.
            if (ReadErrors >= MAXREADERRORS) {
                esyslog("ERROR: dvd-plugin giving up after %d consecutive read errors", ReadErrors);
                running = false;
            } else {
                dvdnav_reset(nav);
            }
            continue;
        }
        // Only data read from the disc counts as recovered: after a reset
        // libdvdnav reports the new title set and cell from the IFO files.
        if (event == DVDNAV_BLOCK_OK || event == DVDNAV_NAV_PACKET) {
            ReadErrors = 0;
        }
        noAudio = playMode != ePlayMode::Play;

        switch (event) {
            case DVDNAV_BLOCK_OK:
                UpdateBlockInfo();
                playedPacket = playPacket(cache_ptr, trickMode, noAudio);
                break;
            case DVDNAV_NOP:
                DEBUG_NAV("%s:%d:NAV NOP\n", __FILE__, __LINE__);
                break;
            case DVDNAV_STILL_FRAME: {
                const uint64_t currentTicks = time_ticks();
                DEBUG_PTS2("%s:%d:NAV STILL FRAME (menu=%d), rem. stillTimer=%llut(%llums), currTicks=%llut(%llums)\n",
                           __FILE__, __LINE__, IsInMenuDomain(), static_cast<unsigned long long>(stillTimer),
                           static_cast<unsigned long long>(stillTimer) / 90,
                           static_cast<unsigned long long>(currentTicks),
                           static_cast<unsigned long long>(currentTicks) / 90);
                dvdnav_still_event_t still;
                memcpy(&still, cache_ptr, sizeof(still));
                if (stillTimer != 0) {
                    seenVPTS(0);
                    if (stillTimer > currentTicks) {
                        sleept = uint64_t{40} * 90; // 40 ms
                    } else {
                        StillSkip();
                        sleept = 0;
                        DEBUG_PTS("Still time clear\n");
                    }
                    DEBUG_PTS2("StillTimer->Sleep: %llut(%llums)\n", static_cast<unsigned long long>(sleept),
                               static_cast<unsigned long long>(sleept) / 90);
                } else {
                    if (still.length == 0xff) {
                        stillTimer = UINT64_MAX; // until the user leaves it
                        DEBUG_PTS("Still time (max)\n");
                    } else {
                        stillTimer = currentTicks + (static_cast<uint64_t>(still.length) * 90000U);
                        DEBUG_PTS("Still time (set): %llut(%llums)\n", static_cast<unsigned long long>(stillTimer),
                                  static_cast<unsigned long long>(stillTimer) / 90);
                    }
                    SendIframe(true);
                }
                break;
            }

            case DVDNAV_WAIT:
                DEBUG_NAV("%s:%d:NAV WAIT\n", __FILE__, __LINE__);
                prev_e_ptm = 0;
                ptm_offs = 0;
                dvdnav_wait_skip(nav);
                break;

            case DVDNAV_SPU_STREAM_CHANGE: {
                DEBUG_NAV("%s:%d:NAV SPU STREAM CHANGE\n", __FILE__, __LINE__);
                dvdnav_spu_stream_change_event_t ev;
                memcpy(&ev, cache_ptr, sizeof(ev));
                DEBUG_SUBP_ID("SPU Streams: w:0x%X (%d), l:0x%X (%d), p:0x%X (%d), locked=%d\n", ev.physical_wide,
                              ev.physical_wide, ev.physical_letterbox, ev.physical_letterbox, ev.physical_pan_scan,
                              ev.physical_pan_scan, currentNavSubpStreamUsrLocked);

                if (IsInMenuDomain() || IsDvdNavigationForced() || !currentNavSubpStreamUsrLocked) {
                    const int previousNavSubpStream = currentNavSubpStream;
                    const cSpuDecoder::eScaleMode mode =
                        SPUdecoder ? SPUdecoder->getScaleMode() : cSpuDecoder::eSpuNormal;
                    // libdvdnav gives the physical streams of the active logical
                    // one, -1 for none; bit 7 set means hide, and only let forced
                    // subtitles show (see vm.c from libdvdnav). The logical
                    // number it leaves out.
                    int physical = ev.physical_wide;
                    if (mode == cSpuDecoder::eSpuLetterBox) { // TV 4:3, DVD 16:9
                        physical = ev.physical_letterbox;
                    } else if (mode == cSpuDecoder::eSpuPanAndScan) {
                        physical = ev.physical_pan_scan;
                    }
                    currentNavSubpPhysical = physical < 0 ? -1 : physical & SubpStreamMask;
                    currentNavSubpStream = -1;
                    if (ev.physical_wide >= 0) {
                        currentNavSubpStream = LogicalSubpStream(ev.physical_wide & SubpStreamMask);
                        if (currentNavSubpStream < 0) { // a menu's
                            currentNavSubpStream = ev.physical_wide & SubpStreamMask;
                        }
                        currentNavSubpStream |= ev.physical_wide & 0x80;
                    }
                    DEBUG_SUBP_ID("dvd choosing SPU stream: curNavSpu=0x%X, physical %d (mode %d)\n",
                                  currentNavSubpStream, currentNavSubpPhysical, mode);
                    // A menu's subpicture stream holds its buttons, not subtitles.
                    if (currentNavSubpStream != previousNavSubpStream && !IsInMenuDomain()) {
                        ReportSubtitleTrack();
                    }
                } else {
                    DEBUG_SUBP_ID("DVDNAV_SPU_STREAM_CHANGE: ignore (locked=%d|not enabled=%d), menu=%d, feature=%d \n",
                                  currentNavSubpStreamUsrLocked, DVDSetup.ShowSubtitles, IsInMenuDomain(), isInFeature);
                }
                break;
            }
            case DVDNAV_AUDIO_STREAM_CHANGE: {
                DEBUG_NAV("%s:%d:NAV AUDIO STREAM CHANGE\n", __FILE__, __LINE__);
                if (!currentNavAudioTrackUsrLocked) {
                    const int id = dvdnav_get_active_audio_stream(nav);
                    if (id >= 0) {
                        // Only Dolby Digital and DTS are Dolby tracks for VDR; it
                        // plays MPEG audio and LPCM as ttAudio, the track type has
                        // to match or VDR drops the stream.
                        const int logical = LogicalAudioStream(id);
                        const uint16_t format = logical >= 0
                                                    ? dvdnav_audio_stream_format(nav, static_cast<uint8_t>(logical))
                                                    : static_cast<uint16_t>(DVD_AUDIO_FORMAT_AC3);
                        const bool dolby = format == DVD_AUDIO_FORMAT_AC3 || format == DVD_AUDIO_FORMAT_DTS;
                        DEBUG_AUDIO_ID("dvd->SetCurrentAudioTrack %s %02X\n", dolby ? "DOLBY" : "AUDIO", id);
                        eTrackType track = ttNone;
                        if (dolby) {
                            const int subStreamId = (format == DVD_AUDIO_FORMAT_DTS ? aDTS : aAC3) | id;
                            track = static_cast<eTrackType>(ttDolby + DolbyTrackIndex(subStreamId));
                        } else {
                            track = static_cast<eTrackType>(ttAudio + id);
                        }
                        // libdvdnav announces the active stream at every cell.
                        // VDR switches to a Dolby track again even if it is the
                        // current one, and the device can only reset its audio
                        // for that: with several Dolby tracks it can't tell
                        // which one comes, so every cell cut a second of sound.
                        if (track != currentAudioTrack) {
                            DeviceSetCurrentAudioTrack(track);
                        }
                    }
                    currentNavAudioTrack = id;

                    DEBUG_AUDIO_ID("DVDNAV_AUDIO_STREAM_CHANGE: curNavAu=%d 0x%02X\n", id, id);
                    SetCurrentNavAudioTrackUsrLocked(false);
                } else {
                    DEBUG_AUDIO_ID("DVDNAV_AUDIO_STREAM_CHANGE: ignore (locked)\n");
                }
                break;
            }
            case DVDNAV_VTS_CHANGE:
                DEBUG_NAV("%s:%d:NAV VTS CHANGE -> Empty, setAll-spu&audio \n", __FILE__, __LINE__);
                // Let the device play out what it holds of the title set that ends,
                // or the clip in front of the next one - such as a menu's way into
                // the film - lost its end. Not after a jump: that emptied the player.
                if (playMode == ePlayMode::Play && (cntVidBlocksPlayed > 0 || cntAudBlocksPlayed > 0)) {
                    WaitDeviceDrained();
                }
                if (cntVidBlocksPlayed > 0) {
                    firstClear = false;
                }
                Empty(true);
                UpdateBlockInfo();
                UpdateVTSInfo();
                // Audio first: its clear removes the subtitle tracks from VDR too.
                setAllAudioTracks();
                setAllSubpStreams();
                SetTitleInfoString();
                SetTitleString();
                SetAspectString();
                if (first_vts_change) {
                    first_vts_change = false;

                    // The resume key needs the disc's titles, known now. The
                    // second follows at the next cell change: before the jump
                    // is there, libdvdnav fails a sector search with "New
                    // position not yet determined".
                    int resTitle = 0;
                    int resChapter = 0;
                    if (LoadResume(resTitle, resChapter, resSecond)) {
                        next_cell_change = GotoTitle(resTitle, resChapter) > 0;
                    }
                }
                break;
            case DVDNAV_CELL_CHANGE: {
                DEBUG_NAV("%s:%d:NAV CELL CHANGE\n", __FILE__, __LINE__);
                if (cntVidBlocksPlayed > 0) {
                    firstClear = false;
                }

                memcpy(&lastCellEventInfo, cache_ptr, sizeof(dvdnav_cell_change_event_t));
                UpdateBlockInfo();
                UpdateVTSInfo();
                BlocksToPGCTicks(1, pgcTicksPerBlock, pgcTotalTicks);
#ifdef CTRLDEBUG
                printCellInfo(lastCellEventInfo, pgcTicksPerBlock);
#endif
                // The cell that ended may have ended in a still picture.
                SendIframe((stillFrame & CELL_STILL) != 0);
                stillFrame = (dvdnav_get_next_still_flag(nav) != 0) ? CELL_STILL : 0;
                SetTitleInfoString();
                if (next_cell_change) {
                    next_cell_change = false;
                    Goto(static_cast<int>(resSecond));
                }
                break;
            }
            case DVDNAV_NAV_PACKET: {
                DEBUG_NAV0("%s:%d:NAV PACKET\n", __FILE__, __LINE__);
                current_pci = dvdnav_get_current_nav_pci(nav);
                menuButtons = current_pci != nullptr && current_pci->hli.hl_gi.hli_ss != 0;
                DEBUG_NAV0("NAV: %x, prev_e_ptm: %8d, s_ptm: %8d, ", stillFrame, prev_e_ptm,
                           current_pci->pci_gi.vobu_s_ptm);
                if (prev_e_ptm) {
                    ptm_offs += prev_e_ptm - static_cast<int>(current_pci->pci_gi.vobu_s_ptm);
                }
                DEBUG_NAV0("ptm_offs: %8d\n", ptm_offs);
                prev_e_ptm = static_cast<int>(current_pci->pci_gi.vobu_e_ptm);
                if (!IsInMenuDomain()) {
                    NoteVobu(current_pci);
                }
                if (current_pci && (current_pci->hli.hl_gi.hli_ss & 0x03) == 1) {
                    UpdateButtonHighlight(nullptr);
                }
                const dsi_t *dsi = dvdnav_get_current_nav_dsi(nav);
                SendIframe((stillFrame & NAV_STILL) != 0);
                if (dsi->vobu_sri.next_video == 0xbfffffff) {
                    stillFrame |= NAV_STILL;
                } else {
                    stillFrame &= static_cast<uint8_t>(~NAV_STILL);
                }
                break;
            }
            case DVDNAV_STOP:
                DEBUG_NAV("%s:%d:NAV STOP\n", __FILE__, __LINE__);
                running = false;
                break;
            case DVDNAV_HIGHLIGHT: {
                DEBUG_NAV("%s:%d:NAV HIGHLIGHT\n", __FILE__, __LINE__);
                dvdnav_highlight_event_t hlevt;
                memcpy(&hlevt, cache_ptr, sizeof(hlevt));
                UpdateButtonHighlight(&hlevt);
                break;
            }
            case DVDNAV_SPU_CLUT_CHANGE:
                DEBUG_NAV("%s:%d:NAV SPU CLUT CHANGE SPUdecoder=%d\n", __FILE__, __LINE__, SPUdecoder != nullptr);
                if (SPUdecoder) {
                    ClearButtonHighlight();
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- C-API: the block is the CLUT
                    SPUdecoder->setPalette(reinterpret_cast<uint32_t *>(cache_ptr));
                }
                break;
            case DVDNAV_HOP_CHANNEL:
                DEBUG_NAV("%s:%d:NAV HOP CHANNEL -> Empty\n", __FILE__, __LINE__);
                // Not in a trick mode, whose own jumps cause the hop: Empty()
                // would reset the count of blocks played, which backward play
                // jumps by.
                if (!trickMode) {
                    Empty(true);
                }
                UpdateBlockInfo();
                UpdateVTSInfo();
                break;
            default:
                DEBUG_NAV("%s:%d:NAV unknown event %d\n", __FILE__, __LINE__, event);
                break;
        }
#ifdef DVDDEBUG
        if ((event != DVDNAV_BLOCK_OK) && (event != DVDNAV_NAV_PACKET) && (event != DVDNAV_STILL_FRAME)) {
            DEBUGDVD("got event (%d)%s, len %d\n", event, EventName(event), len);
        }
#endif
        if (cache_ptr != nullptr && cache_ptr != event_buf.data()) {
            dvdnav_free_cache_block(nav, cache_ptr);
        }
        if (trackInfoTimeout.TimedOut()) {
            UpdateTrackInfo();
            trackInfoTimeout.Set(TRACKINFOINTERVALMS);
        }
    }

    DEBUG_NAV("%s:%d: empty\n", __FILE__, __LINE__);

    if (navFailed) {
        Skins.QueueMessage(mtError, tr("Error.DVD$Error fetching data from DVD!"));
    }
    Empty();

    running = false;

    {
        // The control's thread uses nav and SPUdecoder under this lock.
        LOCK_THREAD;
        SPUdecoder = nullptr;
        menuButtons = false;
        current_pci = nullptr; // points into nav
        dvdnav_close(nav);
        nav = nullptr;
        UpdateTrackInfo();
        const cMutexLock MutexLock(&infoMutex);
        infoPositionValid = false;
    }

    DEBUGDVD("%s:%d: input thread ended (pid=%d)\n", __FILE__, __LINE__, getpid());

    // Last: DVDRemoveable() now tells the control to end the replay.
    active = false;
}

#if DVDNAV_VERSION >= DVDNAV_VERSION_CODE(6, 1, 0)
void cDvdPlayer::NavLog(void *Player, dvdnav_logger_level_t Level, const char *Format, va_list Args) {
    std::array<char, 256> buffer{};
    (void)vsnprintf(buffer.data(), buffer.size(), Format, Args); // a longer note is cut
    const char *msg = buffer.data();
    auto *player = static_cast<cDvdPlayer *>(Player);
    // libdvdnav repeats its notes: on a disc without menus in the menu
    // language every jump to a menu brought "Language 'de' not found, using
    // 'en' instead" and the disc's menu languages, hundreds of lines in an
    // evening. Log a note once a replay, an error each time.
    if (Level != DVDNAV_LOGGER_LEVEL_ERROR && !player->NavLogFirst(msg)) {
        return;
    }
    switch (Level) {
        case DVDNAV_LOGGER_LEVEL_ERROR:
            esyslog("ERROR: dvd-plugin libdvdnav: %s", msg);
            break;
        case DVDNAV_LOGGER_LEVEL_WARN:
            isyslog("dvd-plugin libdvdnav: %s", msg);
            break;
        case DVDNAV_LOGGER_LEVEL_INFO:
            dsyslog("dvd-plugin libdvdnav: %s", msg);
            break;
        default:
            break;
    }
    // libdvdnav reports a title set it fails to open (unreadable IFO file)
    // only here. It has switched to the title domain and freed the previous
    // title set by then, so any further call reads freed memory or a NULL
    // pointer: stop asking it anything, before the call that logged this
    // even returns.
    if (Level == DVDNAV_LOGGER_LEVEL_ERROR &&
        (startswith(msg, "ifoOpenVTSI failed") || (startswith(msg, "ifoRead_") && strstr(msg, " failed")))) {
        player->navFailed = true;
        player->running = false;
    }
}

auto cDvdPlayer::NavLogFirst(const char *Msg) -> bool {
    const cMutexLock MutexLock(&navLogMutex);
    if (navLogged.Find(Msg) >= 0) {
        return false;
    }
    // A bound for a disc with endless different notes, which are logged then.
    if (navLogged.Size() < 100) {
        navLogged.Append(strdup(Msg)); // cStringList frees it
    }
    return true;
}
#endif

void cDvdPlayer::ClearButtonHighlight() {
    LOCK_THREAD;

    if (SPUdecoder) {
        SPUdecoder->clearHighlight();
        DEBUG_NAV("DVD NAV SPU clear button\n");
    }
}

void cDvdPlayer::activateButton() {
    DEBUGDVD("activateButton\n");
    LOCK_THREAD;
    if (nav && current_pci) {
        int buttonN = 0;
        dvdnav_highlight_area_t hl;

        dvdnav_get_current_highlight(nav, &buttonN);

        if (dvdnav_get_highlight_area(current_pci, buttonN, 0, &hl) == DVDNAV_STATUS_OK) {

            ClearButtonHighlight();

            DEBUG_NAV("DVD NAV SPU activate hl button: %d: %d/%d %d/%d (%dx%d)\n", buttonN, hl.sx, hl.sy, hl.ex, hl.ey,
                      hl.ex - hl.sx + 1, hl.ey - hl.sy + 1);
        }
        dvdnav_button_select_and_activate(nav, current_pci, buttonN);
        subpMenuChoice = true;
    }
}

void cDvdPlayer::UpdateButtonHighlight(const dvdnav_highlight_event_t *hlevt) {
    LOCK_THREAD;

    if (hlevt) {
        DEBUG_NAV("DVD NAV SPU highlight evt: %d: %d/%d %d/%d (%dx%d)\n", hlevt->buttonN, hlevt->sx, hlevt->sy,
                  hlevt->ex, hlevt->ey, hlevt->ex - hlevt->sx + 1, hlevt->ey - hlevt->sy + 1);
    }

    int buttonN = -1;

    dvdnav_get_current_highlight(nav, &buttonN);

    ClearButtonHighlight();

    if (SPUdecoder && current_pci && TakeOsd()) {
        dvdnav_highlight_area_t hl;

        // libdvdnav ignores the button groups, which letterbox and pan & scan
        // need for the right highlight.
        bool libdvd_workaround = false;

        // Button 0 is none: with it, or a number beyond its group, the index
        // went before or past the group's buttons.
        const int btns_per_group = current_pci->hli.hl_gi.btngr_ns ? 36 / current_pci->hli.hl_gi.btngr_ns : 0;
        if (buttonN >= 1 && buttonN <= btns_per_group) {
            const btni_t *button_ptr = nullptr;
            const int modeMask = 1 << SPUdecoder->getScaleMode();

            // libdvdnav's C arrays: a group holds 36 / btngr_ns buttons, so the
            // index stays below 36; a colour number other than 0 is 1 to 3.
            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
            if (current_pci->hli.hl_gi.btngr1_dsp_ty & modeMask) {
                button_ptr = &current_pci->hli.btnit[(0 * btns_per_group) + buttonN - 1];
            }
            if (!button_ptr && current_pci->hli.hl_gi.btngr_ns >= 2 &&
                (current_pci->hli.hl_gi.btngr2_dsp_ty & modeMask)) {
                button_ptr = &current_pci->hli.btnit[(1 * btns_per_group) + buttonN - 1];
            }
            if (!button_ptr && current_pci->hli.hl_gi.btngr_ns >= 3 &&
                (current_pci->hli.hl_gi.btngr3_dsp_ty & modeMask)) {
                button_ptr = &current_pci->hli.btnit[(2 * btns_per_group) + buttonN - 1];
            }

            if (button_ptr && button_ptr->btn_coln != 0) {
                const auto sx = static_cast<int>(button_ptr->x_start);
                auto ex = static_cast<int>(button_ptr->x_end);
                if ((ex - sx + 1) & 0x03) {
                    ex += 4 - ((ex - sx + 1) & 0x03);
                }

                SPUdecoder->setHighlight(static_cast<uint16_t>(sx), button_ptr->y_start, static_cast<uint16_t>(ex),
                                         button_ptr->y_end,
                                         current_pci->hli.btn_colit.btn_coli[button_ptr->btn_coln - 1][0]);
                libdvd_workaround = true;
            }
            // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
        }

        if (!libdvd_workaround && dvdnav_get_highlight_area(current_pci, buttonN, 0, &hl) == DVDNAV_STATUS_OK) {
            DEBUG_NAV("DVD NAV SPU highlight button: %d: %d/%d %d/%d (%dx%d)\n", buttonN, hl.sx, hl.sy, hl.ex, hl.ey,
                      hl.ex - hl.sx + 1, hl.ey - hl.sy + 1);

            if ((hl.ex - hl.sx + 1) & 0x03) {
                hl.ex = static_cast<uint16_t>(hl.ex + 4 - ((hl.ex - hl.sx + 1) & 0x03));
            }
            DEBUG_NAV("\t\t-> %d/%d %d/%d (%dx%d)\n", hl.sx, hl.sy, hl.ex, hl.ey, hl.ex - hl.sx + 1, hl.ey - hl.sy + 1);

            SPUdecoder->setHighlight(hl.sx, hl.sy, hl.ex, hl.ey, hl.palette);

            if (pktpts != 0xFFFFFFFF) {
                seenVPTS(pktpts);
            }
        } else {
            // no button selected yet: the first
            if (buttonN == 0) {
                DEBUG_NAV("DVD NAV SPU select button 1 %s:%d\n", __FILE__, __LINE__);
                dvdnav_button_select(nav, current_pci, 1);
            }
        }
    } else {
        DEBUG_NAV("not current pci button: %d, SPUdecoder=%d, current_pci=%p\n", buttonN, SPUdecoder != nullptr,
                  static_cast<const void *>(current_pci));
    }
}

// The aspect ratio of a sequence header, as the DVD allows it on the TV:
// 1 = square pixels, 2 = 4:3, 3 = 16:9, 4 = 2.21:1.
void cDvdPlayer::DoScaleMode(int &vaspect) const {
    // Only for a DVD that is not 4:3, on a 4:3 TV.
    if (!Setup.VideoFormat && dvd_aspect != 0) {
        if (vaspect != 2) {
            if (!(dvd_scaleperm & 1)) { // letterbox allowed: keep 16:9
                vaspect = 0x03;
            } else if (!(dvd_scaleperm & 2)) { // pan & scan allowed
                vaspect = 0x02;                // 4:3
            }
        }
        if (vaspect == 2 && dvd_aspect == 3) { // letterbox, as the DVD's aspect ratio says
            vaspect = 0x03;                    // 16:9
        }
    }
}

void cDvdPlayer::seenVPTS(uint64_t pts) { seenSpuPts(pts); }

void cDvdPlayer::seenAPTS(uint64_t pts) { seenSpuPts(pts); }

void cDvdPlayer::seenSpuPts(uint64_t pts) {
    LOCK_THREAD;
    if (SPUdecoder && TakeOsd()) {
        SPUdecoder->setTime(static_cast<uint32_t>(pts));
    }
}

void cDvdPlayer::SendIframe(bool doSend) {
    if (IframeCnt == 0 && iframeAssembler->Available() && doSend) {
        DEBUG_IFRAME("I-Frame: Doing StillFrame: IframeCnt=%d->1, used=%d, doSend=%d, stillFrame=0x%X\n", IframeCnt,
                     iframeAssembler->Available(), doSend, stillFrame);
        IframeCnt = 1;
    }
}

void cDvdPlayer::playSPU(int spuId) {
    // Where a broken unit ends is unknown: drop what there is. With a size
    // of 0 the unit took nothing from the FIFO, which stayed stuck, and the
    // SPU decoder read its header from an empty buffer.
    if (!SPUassembler.valid()) {
        DEBUG_SUBP_ID("playSPU: broken SPU, size %d, control at %d: dropped\n", SPUassembler.getSize(),
                      SPUassembler.getCmdOffs());
        SPUassembler.Reset();
        return;
    }
    const int spuSize = SPUassembler.getSize();
    const auto bufferSize = static_cast<size_t>(spuSize);
    auto *buffer = new uint8_t[bufferSize];
    SPUassembler.Get(buffer, spuSize);
    // Without allowedShow, the SPU decoder shows the forced subpictures only,
    // such as a menu's buttons.
    const bool allowedShow = IsDvdNavigationForced() || SubtitlesShown(spuId);

    DEBUG_SUBP_ID("playSPU: SPU proc, spu_size:%d, menuDomain=%d, pts: %12llu\n", spuSize, IsInMenuDomain(),
                  static_cast<unsigned long long>(SPUassembler.getPts()));

    SPUdecoder->processSPU(static_cast<uint32_t>(SPUassembler.getPts()), buffer, allowedShow);
    if (IsInMenuDomain()) {
        seenVPTS(pktpts); // in a title the video's PTS follow
    }
}

void cDvdPlayer::SetAvailableAudioTrack(eTrackType Type, int Index, uint16_t Id) {
    // DVDs give a track's language as an ISO 639-1 code. That is the track's
    // language, which only holds a few bytes and is what VDR's audio menu
    // shows; the language's name goes into the description. The stream's
    // number is the low bits of its id, also where Index is a DTS track's.
    const uint16_t languageCode = GetAudioTrackLanguageCode(Id & AC3AudioTrackMask);
    // This runs for every audio packet, and VDR's audio menu reads a track's
    // language and description without a lock: write them only once.
    const auto track = static_cast<eTrackType>(Type + Index);
    if (track > ttNone && track < ttMaxTrackTypes) {
        if (announcedLanguageCode.at(track) == languageCode) {
            DeviceSetAvailableTrack(Type, Index, Id);
            return;
        }
        announcedLanguageCode.at(track) = languageCode;
    }
    LanguageLetters language{};
    const char *languageName = TrackLanguage(languageCode, language);
    const cString trackDescription = AudioTrackDescription(Id & AC3AudioTrackMask, languageName);
    DeviceSetAvailableTrack(Type, Index, Id, language.at(0) ? language.data() : nullptr, trackDescription);
}

void cDvdPlayer::AnnounceSubtitleTrack(int Stream) {
    // VDR's subtitle menu lists the subtitle tracks; this player draws the
    // subtitles itself, VDR never gets their packets.
    LanguageLetters language{};
    const char *languageName = TrackLanguage(GetSubtitleLanguageCode(Stream), language);
    const cString trackDescription = SubtitleTrackDescription(Stream, languageName);
    subtitleTrackSync = true;
    DeviceSetAvailableTrack(ttSubtitle, Stream, static_cast<uint16_t>(0x20 | Stream),
                            language.at(0) ? language.data() : nullptr, trackDescription);
    subtitleTrackSync = false;
}

auto cDvdPlayer::SubtitlesShown(int Stream) const -> bool {
    // Whether all of Stream's subtitles are shown, not just the forced ones:
    // the viewer chose it with key 2 or VDR's menu, or the disc shows it (bit
    // 7 clear) and that may count. "Display subtitles: no" hides what a disc
    // switches on by itself - not what the viewer chose in the disc's menu,
    // where the disc shows the selected stream the same way.
    if (Stream < 0) {
        return false;
    }
    if (currentNavSubpStreamUsrLocked) {
        return true;
    }
    return (Stream & 0x80) == 0 && (DVDSetup.ShowSubtitles != 0 || subpMenuChoice);
}

void cDvdPlayer::ReportSubtitleTrack() {
    // As a manual choice, so that VDR doesn't pick a track of its own when
    // one is announced: the disc or the user chose this one. A stream that
    // shows forced subtitles only is what VDR's menu calls "none".
    const int stream = currentNavSubpStream;
    const eTrackType type =
        SubtitlesShown(stream) ? static_cast<eTrackType>(ttSubtitleFirst + (stream & SubpStreamMask)) : ttNone;
    subtitleTrackSync = true;
    cDevice::PrimaryDevice()->SetCurrentSubtitleTrack(type, true);
    subtitleTrackSync = false;
}

void cDvdPlayer::SetSubtitleTrack(eTrackType Type, const tTrackId * /*TrackId*/) {
    LOCK_THREAD; // also orders the flag with the player thread setting it
    // While the player announces or reports a track, VDR's call back only
    // echoes the player's own state, or VDR's guess at one.
    if (subtitleTrackSync || !DVDActiveAndRunning()) {
        return;
    }
    SetSubpStream(IS_SUBTITLE_TRACK(Type) ? Type - ttSubtitleFirst : -1);
}

auto cDvdPlayer::playPacket(unsigned char *&cache_buf, bool trickMode, bool noAudio) -> int {
    int playedPacket = pktNone;
    unsigned char *sector = cache_buf;

    // Every block of a VOB is a pack and starts with the pack header. A
    // search for a start code further in parsed the rest of the block from
    // there as if it were all of it, that many bytes past its end.
    if (!cPStream::packetStart(sector) || cPStream::packetType(sector) != 0xBA) {
        esyslog("ERROR: dvd-plugin got unexpected packet: %x %x %x %x", sector[0], sector[1], sector[2], sector[3]);
        return playedPacket;
    }

    const int offset = 14 + cPStream::stuffingLength(sector);
    sector += offset;
    const int r = DVD_VIDEO_LB_LEN - offset;

    sector[6] &= 0x8f;

    uchar *data = sector;
    int datalen = r;

    // MPEG video and audio streams have a range of ids each.
    int streamType = cPStream::packetType(sector);
    if (VIDEO_STREAM_S <= streamType && streamType <= VIDEO_STREAM_E) {
        streamType = VIDEO_STREAM_S;
    } else if (AUDIO_STREAM_S <= streamType && streamType <= AUDIO_STREAM_E) {
        streamType = AUDIO_STREAM_S;
    }

    switch (streamType) {
        case VIDEO_STREAM_S: {
            const bool ptsFlag = (sector[7] & 0x80) != 0 && sector[8] >= 5;
            if (ptsFlag) {
                pktpts = cPStream::fromPTS(sector + 9) + static_cast<uint64_t>(ptm_offs);
            }

            // skip the optional and the mandatory header bytes
            data += cPStream::PESHeaderLength(sector);
            data += 3;
            // What is left of the packet from data on, which starts 6 bytes
            // before the payload: the scan below must not run past it. The
            // length without the header data counted 3 bytes of the next block,
            // and a sequence header starting at the end of a packet had its
            // aspect ratio "corrected" there, in libdvdnav's read cache.
            datalen = min(cPStream::packetLength(sector), r) - static_cast<int>(data - sector);

            // Trick modes play I-frames only, from their start: not the rest of
            // a picture that was under way when the trick mode began.
            if (trickMode && videoPictureType != I_FRAME) {
                skipPlayVideo = true;
            }
            const uchar *payload = sector + 9 + cPStream::PESHeaderLength(sector);
            const uchar *firstHeader = nullptr; // of the picture that starts in this packet
            // Bytes to blank in the copy that is played (the block itself is
            // in libdvdnav's read cache), as offsets into the packet.
            int blankFrom = 0;
            int blankTo = 0;

            uint8_t currentFrameType = 0;
            const bool do_copy = (lastFrameType == I_FRAME) && (data[0] != 0 || data[1] != 0 || data[2] != 1);
            bool havePictureHeader = false;
            bool haveSliceBeforePicture = false;
            while (datalen >= 6) { // a picture header's type is in its 6th byte
                if (data[0] == 0 && data[1] == 0 && data[2] == 1) {
                    const int ptype2 = cPStream::packetType(data);
                    if (!firstHeader && (ptype2 == SEQUENCE_HEADER || ptype2 == GOP_START || ptype2 == SC_PICTURE)) {
                        firstHeader = data;
                    }
                    if (ptype2 == SEQUENCE_HEADER && trickMode && skipPlayVideo) {
                        // A DVD has a sequence header in front of every GOP,
                        // which starts with an I-frame: play from that header on,
                        // after a clear the decoder can't decode the I-frame
                        // without it.
                        blankFrom = static_cast<int>(payload - sector);
                        blankTo = static_cast<int>(data - sector);
                        skipPlayVideo = false;
                        videoPictureType = I_FRAME;
                    }
                    if (ptype2 == SC_PICTURE) {
                        const uchar foundFrameType = static_cast<uchar>(data[5] >> 3) & 0x07;
                        if (foundFrameType < I_FRAME || B_FRAME < foundFrameType) {
                            data++;
                            datalen--;
                            continue;
                        }
                        havePictureHeader = true;
                        VideoPts += 3600;
                        lastFrameType = foundFrameType;
                        videoPictureType = foundFrameType;
                        if (!currentFrameType) {
                            currentFrameType = lastFrameType;
                        }
                        // Trick modes play I-frames only.
                        const bool skip = lastFrameType > I_FRAME && trickMode;
                        if (skip && !skipPlayVideo) {
                            // The I-frame being played ends in this packet, its
                            // last slices before this picture: play them, blank
                            // the rest (zero bytes are stuffing before a start
                            // code); the next packet is skipped. Skipping this
                            // one cut the bottom rows off every I-frame.
                            blankFrom = static_cast<int>(data - sector);
                            blankTo = cPStream::packetLength(sector);
                            break;
                        }
                        if (!skip && skipPlayVideo && firstHeader > payload) {
                            // The next I-frame starts after the end of a
                            // skipped picture, which the decoder can't use.
                            blankFrom = static_cast<int>(payload - sector);
                            blankTo = static_cast<int>(firstHeader - sector);
                        }
                        skipPlayVideo = skip;

                        data += 5;
                        datalen -= 5;
                    } else if (ptype2 == SEQUENCE_HEADER && datalen >= 8) {
                        // A still picture is the I-frame after the last header.
                        iframeAssembler->Clear();
                        data += 4; // the start code
                        // Correct the aspect ratio. The next 4 bytes as nibbles:
                        // ww|wh|hh|ar - width, height, aspect ratio, frame rate
                        int aspect = (data[3] & 0xf0) >> 4;

                        DoScaleMode(aspect);

                        data[3] &= 0x0f;
                        data[3] |= static_cast<uchar>((aspect & 0x0f) << 4);

                        data += 3;
                        datalen -= 7;
                    } else if (ptype2 == PADDING_STREAM) {
                        DEBUGDVD("PADDING_STREAM @ %d\n", static_cast<int>(data - sector));
                        break;
                    } else if (0x01 <= ptype2 && ptype2 <= 0xaf && !havePictureHeader &&
                               iframeAssembler->Available() == 0) {
                        haveSliceBeforePicture = true; // a slice, before a picture header
                    }
                }
                data++;
                datalen--;
            }

            if (stillFrame && (currentFrameType <= I_FRAME || do_copy)) {
                if (haveSliceBeforePicture) {
                    DEBUG_IFRAME2("I-Frame: clr  (%d,%d,c:%d,p:%d,x:%d,v:%u) !\n", currentFrameType, lastFrameType,
                                  static_cast<int>(do_copy), static_cast<int>(havePictureHeader),
                                  static_cast<int>(haveSliceBeforePicture), cntVidBlocksPlayed);
                    lastFrameType = 0xff;
                } else {
                    DEBUG_IFRAME2("I-Frame: Put MB .. %d+%d=", r, iframeAssembler->Available());
                    iframeAssembler->Put(sector, r);
                    DEBUG_IFRAME2("%d (%d,%d,c:%d,p:%d,x:%d,v:%d)\n", iframeAssembler->Available(), currentFrameType,
                                  lastFrameType, do_copy, havePictureHeader, haveSliceBeforePicture,
                                  cntVidBlocksPlayed);
                }
            }

            if (ptsFlag) {
                VideoPts = pktpts;
                cPStream::toPTS(sector + 9, pktpts);
            }

            rframe = new cFrame(sector, r, ftVideo);
            if (blankTo > blankFrom && blankTo <= r) {
                memset(rframe->Data() + blankFrom, 0, static_cast<size_t>(blankTo - blankFrom));
            }

            playedPacket |= pktVideo;
            if (!IsInMenuDomain()) {
                seenVPTS(VideoPts);
            }
            break;
        }
        case AUDIO_STREAM_S: {
            lastFrameType = AUDIO_STREAM_S;
            const int audioVal = cPStream::packetType(sector);
            const int audioId = audioVal & AudioTrackMask;
            notifySeenAudioTrack(audioId);

            SetAvailableAudioTrack(ttAudio, audioId, static_cast<uint16_t>(audioVal));

            // no sound in trick mode
            if (noAudio) {
                return playedPacket;
            }

            if (currentNavAudioTrack != audioId) {
                return playedPacket;
            }

            const bool ptsFlag = ((sector[7] & 0xC0) == 0x80) && sector[8] >= 5;
            if (ptsFlag) {
                pktpts = cPStream::fromPTS(sector + 9) + static_cast<uint64_t>(ptm_offs);
            }

            playedPacket |= pktAudio;
            SetCurrentNavAudioTrackType(aMPEG);

            if (ptsFlag) {
                cPStream::toPTS(sector + 9, pktpts);
            }

            rframe = new cFrame(sector, r, ftAudio);

            if (ptsFlag) {
                seenAPTS(pktpts);
            }

            break;
        }
        case PRIVATE_STREAM1: {
            // Within the block, as for video: a packet's length beyond it
            // had subtitles read past it.
            datalen = min(cPStream::packetLength(sector), r);

            // skip the optional and the mandatory header bytes
            datalen -= cPStream::PESHeaderLength(sector);
            data += cPStream::PESHeaderLength(sector);
            data += 3;
        }
        // fall through
        case PRIVATE_STREAM2: {
            // On a DVD, private stream 2 holds the NAV packets, which libdvdnav
            // passes as DVDNAV_NAV_PACKET; their substreams 0 and 1 end below
            // as unhandled.
            lastFrameType = PRIVATE_STREAM2;

            // no sound, no spu in trick mode
            if (noAudio) {
                SPUassembler.Reset();
                return playedPacket;
            }

            // skip the packet's start code, stream id and length
            data += 6;

            const bool ptsFlag = (sector[7] & 0xC0) != 0 && sector[8] >= 5;
            if (ptsFlag) {
                pktpts = cPStream::fromPTS(sector + 9) + static_cast<uint64_t>(ptm_offs);
            }

            // data now points to the beginning of the payload
            const int subStreamId = (static_cast<int>(*data));
            const int audioType = subStreamId & 0xF8;

            if (audioType == aAC3 || audioType == aDTS || audioType == aLPCM) {
                const int audioId = subStreamId & AC3AudioTrackMask;

                notifySeenAudioTrack(audioId);

                if (audioType == aLPCM) {
                    SetAvailableAudioTrack(ttAudio, audioId, static_cast<uint16_t>(subStreamId));
                } else {
                    SetAvailableAudioTrack(ttDolby, DolbyTrackIndex(subStreamId), static_cast<uint16_t>(subStreamId));
                }

                if (currentNavAudioTrack == audioId) {
                    playedPacket |= pktAudio;
                    SetCurrentNavAudioTrackType(audioType);

                    if (ptsFlag) {
                        cPStream::toPTS(sector + 9, pktpts);
                    }

                    // AC-3, DTS and LPCM go to the device as they are, which
                    // decodes them. VDR before 2.8.3 drops AC-3 and DTS with
                    // "Use Dolby Digital" off.
                    rframe = new cFrame(sector, r, ftDolby);
                    DEBUG_AUDIO_PLAY2("dvd audio menu=%d, apts=%8ums vpts=%8ums len=%d\n", IsInMenuDomain(),
                                      static_cast<unsigned int>(ptsFlag ? pktpts / 90U : 0),
                                      static_cast<unsigned int>(VideoPts / 90U), r);
                    if (ptsFlag) {
                        seenAPTS(pktpts);
                    }
                }
            } else if ((subStreamId & 0xE0) == 0x20) {
                const int subtitleIndex = subStreamId & SubpStreamMask;
                // A title's stream that the list lacks: the stream number in
                // the packet is a physical one.
                if (isInFeature && !(subpPhysicalSeen & (1U << subtitleIndex))) {
                    subpPhysicalSeen |= 1U << subtitleIndex;
                    const int logical = LogicalSubpStream(subtitleIndex);
                    if (logical >= 0) {
                        notifySeenSubpStream(logical);
                    }
                }

                // The physical stream selects the packets, the logical one
                // tells playSPU() with bit 7 whether only forced subtitles show.
                // Not while another OSD is open.
                if (OsdInUse() || currentNavSubpPhysical == -1 || subtitleIndex != currentNavSubpPhysical) {
                    SPUassembler.Skip(subtitleIndex);
                    break;
                }

                data++;
                datalen -= 10; // 3 (mandatory header) + 6 (PS header)

                SPUassembler.Put(data, datalen, ptsFlag, pktpts, subtitleIndex);
                if (SPUdecoder && SPUassembler.ready()) {
                    playSPU(currentNavSubpStream);
                }
            } else {
                DEBUGDVD("PRIVATE_STREAM2 unhandled (a)id: %d 0x%X\n", static_cast<int>(*data),
                         static_cast<int>(*data));
            }
            break;
        }
        case PADDING_STREAM:
            DEBUGDVD("PADDING_STREAM ...\n");
            lastFrameType = PADDING_STREAM;
            break;
        case SYSTEM_HEADER:
        case PROG_STREAM_MAP:
        default: {
            lastFrameType = 0xff;
            esyslog("ERROR: dvd-plugin don't know what to do - packetType: %x", cPStream::packetType(sector));
            DEBUGDVD("don't know what to do - packetType: %x", cPStream::packetType(sector));
            return playedPacket;
        }
    }
    if (rframe && !ringBuffer->Put(rframe)) {
        delete rframe;
    }
    rframe = nullptr;

    return playedPacket;
}

void cDvdPlayer::Activate(bool On) {
    DEBUGDVD("cDvdPlayer::Activate %d->%d\n", active.load(), On);
    if (On) {
        // VDR's audio and subtitle menus clear the descriptions of every track
        // that does not belong to a recording (SetTrackDescriptions()). Keep
        // them, so the descriptions this player sets for the disc's tracks stay
        // visible; the player manages its own track selection, which VDR's
        // EnsureAudioTrack()/EnsureSubtitleTrack() would otherwise do.
        cDevice::PrimaryDevice()->SetKeepTracks(true);
        Start();
    } else {
        // The thread leaves its loop and closes nav itself. Cancel() returns
        // at once if it has ended on its own already.
        running = false;
        Cancel(3);
        active = false;
        cDevice::PrimaryDevice()->SetKeepTracks(false);
    }
}

void cDvdPlayer::Pause() {
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return;
    }
    if (playMode == ePlayMode::Pause || playMode == ePlayMode::Still) {
        Play();
    } else {
        if (playMode == ePlayMode::Fast || (playMode == ePlayMode::Slow && playDir == ePlayDir::Backward)) {
            DEBUG_NAV("%s:%d: empty\n", __FILE__, __LINE__);
            Empty();
        }
        DeviceFreeze();
        playMode = ePlayMode::Pause;
    }
}

void cDvdPlayer::Stop() {
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return;
    }

    if (running && nav) {
        SaveResume();
        dvdnav_stop(nav);
        // Stop() runs twice when the viewer stops: in ProcessKey() and in the
        // control's destructor. The second one returns above.
        running = false;
    }
}

void cDvdPlayer::Play() {
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return;
    }
    if (playMode != ePlayMode::Play) {
        if (playMode == ePlayMode::Still || playMode == ePlayMode::Fast ||
            (playMode == ePlayMode::Slow && playDir == ePlayDir::Backward)) {
            if (DVDSetup.ReadAHead > 0) {
                dvdnav_set_readahead_flag(nav, DVDSetup.ReadAHead);
            }
            DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
            Empty();
        } else if (replayFromPictureOnPlay) {
            ReplayFromPicture(); // an audio track chosen while paused
        }
        DevicePlay();
        playMode = ePlayMode::Play;
        playDir = ePlayDir::Forward;
    }
}

void cDvdPlayer::Forward() {
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return;
    }
    switch (playMode.load()) {
        case ePlayMode::Fast:
            if (Setup.MultiSpeedMode) {
                TrickSpeed(playDir == ePlayDir::Forward ? 1 : -1);
                break;
            }
            if (playDir == ePlayDir::Forward) {
                Play();
                break;
            }
            // fall through
        case ePlayMode::Play:
            DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
            Empty();
            DeviceMute();
            playMode = ePlayMode::Fast;
            playDir = ePlayDir::Forward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? 1 : MAX_SPEEDS);
            break;
        case ePlayMode::Slow:
            if (Setup.MultiSpeedMode) {
                TrickSpeed(playDir == ePlayDir::Forward ? -1 : 1);
                break;
            }
            if (playDir == ePlayDir::Forward) {
                Pause();
                break;
            }
            // fall through
        case ePlayMode::Still:
        case ePlayMode::Pause:
            DeviceMute();
            playMode = ePlayMode::Slow;
            playDir = ePlayDir::Forward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? -1 : -MAX_SPEEDS);
            break;
    }
}

void cDvdPlayer::Backward() {
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return;
    }
    switch (playMode.load()) {
        case ePlayMode::Fast:
            if (Setup.MultiSpeedMode) {
                TrickSpeed(playDir == ePlayDir::Backward ? 1 : -1);
                break;
            }
            if (playDir == ePlayDir::Backward) {
                Play();
                break;
            }
            // fall through
        case ePlayMode::Play:
            DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
            Empty();
            DeviceMute();
            playMode = ePlayMode::Fast;
            playDir = ePlayDir::Backward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? 1 : MAX_SPEEDS);
            break;
        case ePlayMode::Slow:
            if (Setup.MultiSpeedMode) {
                TrickSpeed(playDir == ePlayDir::Backward ? -1 : 1);
                break;
            }
            if (playDir == ePlayDir::Backward) {
                Pause();
                break;
            }
            // fall through
        case ePlayMode::Still:
        case ePlayMode::Pause:
            DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
            Empty();
            DeviceMute();
            playMode = ePlayMode::Slow;
            playDir = ePlayDir::Backward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? -1 : -MAX_SPEEDS);
            break;
    }
}

inline auto cDvdPlayer::GetPGCLengthInTicks() const -> int64_t { return lastCellEventInfo.pgc_length; }

void cDvdPlayer::BlocksToPGCTicks(uint32_t BlockNum, int64_t &Ticks, int64_t &TotalTicks) const {
    TotalTicks = GetPGCLengthInTicks();
    if (pgcTotalBlockNum > 0) {
        Ticks = BlockNum * (TotalTicks / pgcTotalBlockNum);
    } else {
        Ticks = 0;
    }
}

void cDvdPlayer::PGCTicksToBlocks(int64_t Ticks, int64_t &BlockNum, uint32_t &TotalBlockNum) const {
    const int64_t totalTicks = GetPGCLengthInTicks();
    TotalBlockNum = pgcTotalBlockNum;
    BlockNum = totalTicks > 0 ? (Ticks * TotalBlockNum / totalTicks) : 0;
}

auto cDvdPlayer::GetIndex(int &CurrentFrame, int &TotalFrame, bool /*SnapToIFrame*/) -> bool {
    // The position the player thread read last: the control polls this from
    // VDR's main loop, which must not wait for the thread's lock.
    const cMutexLock MutexLock(&infoMutex);
    if (!infoPositionValid || infoTotalBlock == 0) {
        return false;
    }
    CurrentFrame = static_cast<int>(static_cast<double>(infoCurrentBlock) * static_cast<double>(infoPgcLength) /
                                    infoTotalBlock / 90000 * FramesPerSecond());
    TotalFrame = static_cast<int>(static_cast<double>(infoPgcLength) / 90000 * FramesPerSecond());
    return true;
}

auto cDvdPlayer::UpdateBlockInfo() -> bool {
    bool ok = false;
    if (nav) {
        dvdnav_set_PGC_positioning_flag(nav, 1);
        ok = dvdnav_get_position(nav, &pgcCurrentBlockNum, &pgcTotalBlockNum) == DVDNAV_STATUS_OK;
        if (!ok) {
            DEBUG_CONTROL("dvd get pos in title failed (no pos) ..\n");
        }
    } else {
        DEBUG_CONTROL("dvd get pos in title failed (no nav) ..\n");
    }
    const cMutexLock MutexLock(&infoMutex);
    infoPositionValid = ok;
    infoCurrentBlock = pgcCurrentBlockNum;
    infoTotalBlock = pgcTotalBlockNum;
    infoPgcLength = GetPGCLengthInTicks();
    return ok;
}

void cDvdPlayer::UpdateVTSInfo() {
    if (nav) {
        dvd_aspect = dvdnav_get_video_aspect(nav);
        dvd_scaleperm = dvdnav_get_video_scale_permission(nav);
        isInFeature = dvdnav_is_domain_vts(nav) != 0;

#if APIVERSNUM >= 30012 // cSpuDecoder::setResolution(), VDR 2.8.1
        uint32_t dvd_width = 0;
        uint32_t dvd_height = 0;
        dvdnav_get_video_resolution(nav, &dvd_width, &dvd_height);
        if (SPUdecoder) {
            SPUdecoder->setResolution(static_cast<int>(dvd_width), static_cast<int>(dvd_height));
        }
#endif

        eVideoDisplayFormat videoDisplayformat = vdfCenterCutOut;
        if (!(dvd_scaleperm & 1)) {
            videoDisplayformat = vdfLetterBox;
        } else if (!(dvd_scaleperm & 2)) {
            videoDisplayformat = vdfPanAndScan;
        }
        DeviceSetVideoDisplayFormat(videoDisplayformat);
    }
}

void cDvdPlayer::SkipSeconds(int Seconds) {
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return;
    }
    if (Seconds) {
        // In 64 bits: the time search skips up to 99:59 hours. A skip of
        // more than the whole program chain ends at its start or end.
        const int64_t totalTicks = GetPGCLengthInTicks();
        const int64_t diffTicks = max(min(static_cast<int64_t>(Seconds) * 90000, totalTicks), -totalTicks);
        int64_t diffBlockNum = 0;
        uint32_t totalBlockNum = 0;
        PGCTicksToBlocks(diffTicks, diffBlockNum, totalBlockNum);
        PlayFromBlock(pgcCurrentBlockNum + diffBlockNum);
    }
}

void cDvdPlayer::PlayFromBlock(int64_t Block) {
    // As VDR's player does it: from the start of the program chain at the
    // earliest, and a position past its end is ignored. A block number that
    // wrapped around made the seek fail, after the player had been emptied.
    if (Block >= pgcTotalBlockNum) {
        return;
    }
    DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
    Empty();
    if (dvdnav_sector_search(nav, max(Block, int64_t{0}), SEEK_SET) != DVDNAV_STATUS_OK) {
        esyslog("ERROR: dvd-plugin dvd error dvdnav_sector_search: %s", dvdnav_err_to_string(nav));
    }
    Play();
}

auto cDvdPlayer::GetPositionInSec(int64_t &CurrentSec, int64_t &TotalSec) const -> bool {
    int64_t currentTicks = 0;
    int64_t totalTicks = 0;

    if (!DVDActiveAndRunning()) {
        CurrentSec = -1;
        TotalSec = -1;
        return false;
    }

    GetPositionInTicks(currentTicks, totalTicks);
    CurrentSec = PTSTicksToSec(currentTicks);
    TotalSec = PTSTicksToSec(totalTicks);
    return true;
}

void cDvdPlayer::GetPositionInTicks(int64_t &CurrentTicks, int64_t &TotalTicks) const {
    BlocksToPGCTicks(pgcCurrentBlockNum, CurrentTicks, TotalTicks);
}

void cDvdPlayer::Goto(int Seconds) {
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return;
    }

    // The time search asks for 10 s before the end, which is before the
    // start of a shorter program chain.
    int64_t blockNum = 0;
    uint32_t totalBlockNum = 0;
    PGCTicksToBlocks(min(static_cast<int64_t>(max(Seconds, 0)) * 90000, GetPGCLengthInTicks()), blockNum,
                     totalBlockNum);
    PlayFromBlock(blockNum);
}

auto cDvdPlayer::GotoAngle(int Angle) -> int {
    int angleNumber = -1;
    int angleNumbers = -1;

    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return -1;
    }

    dvdnav_get_angle_info(nav, &angleNumber, &angleNumbers);

    if (Angle > angleNumbers) {
        Angle = 1;
    }
    if (Angle <= 0) {
        Angle = angleNumbers;
    }

    // Key 3 in a title with a single angle emptied the player, which cut the
    // picture and the sound with no angle to change to.
    if (Angle == angleNumber) {
        return Angle;
    }

    DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
    Empty();

    dvdnav_angle_change(nav, Angle);

    Play();

    return Angle;
}

void cDvdPlayer::NextAngle() {
    int angleNumber = -1;
    int angleNumbers = -1;

    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return;
    }
    dvdnav_get_angle_info(nav, &angleNumber, &angleNumbers);
    GotoAngle(++angleNumber);
}

auto cDvdPlayer::GotoTitle(int Title, int Chapter) -> int {
    int titleNumbers = 0;
    int chapterNumber = 0;
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return -1;
    }

    // A title the disc doesn't have - a typo in the title search, a resume
    // entry of another disc - is ignored; NextTitle() and PreviousTitle()
    // wrap around themselves.
    if (dvdnav_get_number_of_titles(nav, &titleNumbers) != DVDNAV_STATUS_OK || Title < 1 || Title > titleNumbers) {
        return -1;
    }

    DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
    Empty();

    // A chapter out of range wraps around.
    dvdnav_get_number_of_parts(nav, Title, &chapterNumber);
    if (Chapter > chapterNumber) {
        Chapter = 1;
    }
    if (Chapter <= 0) {
        Chapter = chapterNumber;
    }

    PartPlay(Title, Chapter);

    Play();

    return Title;
}

// The current title, 0 in a menu. Until libdvdnav has opened a title set it
// can't tell, in the menu of a disc that starts there: the title number was
// left unset, and Yellow and Green went to a title the stack chose, or none.
auto cDvdPlayer::CurrentTitle() -> int {
    int title = 0;
    int part = 0;
    return dvdnav_current_title_info(nav, &title, &part) == DVDNAV_STATUS_OK ? title : 0;
}

void cDvdPlayer::NextTitle() {
    int titleNumbers = 0;
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return;
    }
    const int titleNumber = CurrentTitle();
    dvdnav_get_number_of_titles(nav, &titleNumbers);
    GotoTitle(titleNumber < titleNumbers ? titleNumber + 1 : 1);
}

void cDvdPlayer::PreviousTitle() {
    int titleNumbers = 0;
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return;
    }
    const int titleNumber = CurrentTitle();
    dvdnav_get_number_of_titles(nav, &titleNumbers);
    GotoTitle(titleNumber > 1 ? titleNumber - 1 : titleNumbers);
}

auto cDvdPlayer::GotoPart(int Part) -> int {
    int titleNumber = 0;
    int chapterNumber = 0;
    int chapterNumbers = 0;
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return -1;
    }

    // A menu is title 0, which has no chapters: libdvdnav left
    // chapterNumbers unset.
    if (dvdnav_current_title_info(nav, &titleNumber, &chapterNumber) != DVDNAV_STATUS_OK || titleNumber < 1 ||
        dvdnav_get_number_of_parts(nav, titleNumber, &chapterNumbers) != DVDNAV_STATUS_OK) {
        return -1;
    }

    DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
    Empty();

    if (Part > chapterNumbers) {
        Part = 1;
    }
    if (Part <= 0) {
        Part = chapterNumbers;
    }

    PartPlay(titleNumber, Part);
    Play();
    return Part;
}

void cDvdPlayer::NextPart() {
    int titleNumber = 0;
    int chapterNumber = 0;
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return;
    }
    if (dvdnav_current_title_info(nav, &titleNumber, &chapterNumber) == DVDNAV_STATUS_OK) {
        GotoPart(chapterNumber + 1);
    }
}

void cDvdPlayer::PreviousPart() {
    int titleNumber = 0;
    int chapterNumber = 0;
    LOCK_THREAD;
    if (!DVDActiveAndRunning()) {
        return;
    }
    if (dvdnav_current_title_info(nav, &titleNumber, &chapterNumber) == DVDNAV_STATUS_OK) {
        GotoPart(chapterNumber - 1);
    }
}

void cDvdPlayer::clearSeenSubpStream() {
    LOCK_THREAD;

    navSubpStreamSeen.Clear();
    currentNavSubpStream = -1;
    currentNavSubpPhysical = -1;
    subpPhysicalSeen = 0;
    notifySeenSubpStream(-1); // the "off" entry
    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: seen subp cleared\n");
    SetCurrentNavSubpStreamUsrLocked(false);
}

void cDvdPlayer::setAllSubpStreams() {
    clearSeenSubpStream();
    ReportSubtitleTrack();

    // The logical streams of the program chain, with a language or without.
    // Not in a menu, whose subpicture stream holds its buttons: outside a
    // title, libdvdnav gives every logical stream the menu's.
    for (int i = 0; nav != nullptr && isInFeature && i < MaxSubpStreams; i++) {
        if (dvdnav_get_spu_logical_stream(nav, static_cast<uint8_t>(i)) >= 0) {
            notifySeenSubpStream(i);
        }
    }
}

// The index of the stream in the list, bit 7 ignored; -1 if it isn't there.
auto cDvdPlayer::SearchSubpStream(int SubpStreamId) const -> int {
    if (SubpStreamId != -1) {
        SubpStreamId &= SubpStreamMask;
    }
    int i = navSubpStreamSeen.Count() - 1;
    while (i >= 0) {
        // the list holds stream numbers without bit 7, and -1
        if (SubpStreamId == navSubpStreamSeen.Get(i)->getValue()) {
            break; // found
        }
        i--;
    }
    return i;
}

void cDvdPlayer::notifySeenSubpStream(int navSubpStream) {
    // a new stream, and only one the program chain has
    if (SearchSubpStream(navSubpStream) < 0 &&
        (navSubpStream == -1 ||
         (nav && dvdnav_get_spu_logical_stream(nav, static_cast<uint8_t>(navSubpStream)) >= 0))) {
        // The subtitle key steps through this list: keep it in stream
        // order, "off" (-1) first, not in the order the first packets of
        // the streams turned up, which made the key start anywhere.
        IntegerListObject *before = navSubpStreamSeen.First();
        while (before && before->getValue() < navSubpStream) {
            before = navSubpStreamSeen.Next(before);
        }
        if (before) {
            navSubpStreamSeen.Ins(new IntegerListObject(navSubpStream), before);
        } else {
            navSubpStreamSeen.Add(new IntegerListObject(navSubpStream));
        }
        if (navSubpStream >= 0) {
            AnnounceSubtitleTrack(navSubpStream);
        }
    }
}

void cDvdPlayer::SetCurrentNavSubpStreamUsrLocked(bool lock) {
    currentNavSubpStreamUsrLocked = lock;
    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: currentNavSubpStreamUsrLocked=%s\n",
                  currentNavSubpStreamUsrLocked ? "true" : "false");
}

auto cDvdPlayer::GetCurrentNavSubpStream() const -> int { return currentNavSubpStream; }

auto cDvdPlayer::GetSubtitleStreamNumbers() const -> int { return navSubpStreamSeen.Count(); }

void cDvdPlayer::SelectSubpStream() {
    // The viewer chose currentNavSubpStream: show its physical stream, and
    // tell libdvdnav the language for the titles to come.
    currentNavSubpPhysical = PhysicalSubpStream(currentNavSubpStream);
    const uint16_t subtitleLanguageCode = GetSubtitleLanguageCode(currentNavSubpStream);
    if (subtitleLanguageCode != 0xFFFF && nav) {
        LanguageLetters code = Letters(subtitleLanguageCode);
        dvdnav_spu_language_select(nav, code.data());
    }
#if DVDNAV_VERSION >= DVDNAV_VERSION_CODE(7, 0, 0)
    // And the disc's subpicture stream register, as a player's subtitle button
    // sets it and as for the audio track: with the language alone the disc
    // took its first stream of that language again at the next title set.
    // Whether the disc shows the stream or only its forced subtitles stays.
    if (currentNavSubpStream >= 0 && isInFeature && nav) {
        dvdnav_set_active_stream(nav, currentNavSubpStream & SubpStreamMask, DVD_SUBTITLE_STREAM);
    }
#endif
}

void cDvdPlayer::SetSubpStream(int id) {
    LOCK_THREAD;

    const int i = SearchSubpStream(id);

    if (i < 0) {
        id = navSubpStreamSeen.Get(0)->getValue();
    }

    currentNavSubpStream = id;
    SetCurrentNavSubpStreamUsrLocked(true);
    SelectSubpStream();
    UpdateTrackInfo();

    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: curNavSpu next to 0x%X, idx=%d, physical %d, locked=%d\n",
                  currentNavSubpStream, i, currentNavSubpPhysical, currentNavSubpStreamUsrLocked);
}

void cDvdPlayer::NextSubpStream() {
    if (navSubpStreamSeen.Count() == 0) {
        return;
    }

    LOCK_THREAD;

    int i = SearchSubpStream(currentNavSubpStream);

    i = (i + 1) % navSubpStreamSeen.Count();

    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: found curNavSubp(0x%X) at idx = %d\n", currentNavSubpStream, i);

    currentNavSubpStream = navSubpStreamSeen.Get(i)->getValue();
    SetCurrentNavSubpStreamUsrLocked(true);
    SelectSubpStream();
    UpdateTrackInfo();
    ReportSubtitleTrack();
}

void cDvdPlayer::clearSeenAudioTrack() {
    navAudioTracksSeen.Clear();
    DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: seen audio cleared\n");
    SetCurrentNavAudioTrackUsrLocked(false);
    // This rebuilds the track list on a title or cell change, so VDR must drop
    // the old tracks here even though SetKeepTracks(true) otherwise keeps them.
    cDevice::PrimaryDevice()->SetKeepTracks(false);
    DeviceClrAvailableTracks();
    cDevice::PrimaryDevice()->SetKeepTracks(true);
    currentAudioTrack = ttNone; // VDR's clear resets it
    announcedLanguageCode.fill(-1);
}

void cDvdPlayer::setAllAudioTracks() {
    clearSeenAudioTrack();

    for (int i = 0; nav != nullptr && i < MaxAudioTracks; i++) {
        if (GetAudioTrackLanguageCode(i) != 0xFFFF) {
            notifySeenAudioTrack(i);
        }
    }
}

auto cDvdPlayer::SearchAudioStream(int AudioStreamId) const -> int {
    int i = navAudioTracksSeen.Count() - 1;
    while (i >= 0) {
        if (AudioStreamId == navAudioTracksSeen.Get(i)->getValue()) {
            break; // found
        }
        i--;
    }
    return i;
}

void cDvdPlayer::notifySeenAudioTrack(int navAudioTrack) {
    if (SearchAudioStream(navAudioTrack) < 0) {
#ifdef AUDIOIDDEBUG
        uint16_t audioLanguageCode = 0xFFFF;
        int channel_active = 0;
        if (nav != nullptr) {
            audioLanguageCode = GetAudioTrackLanguageCode(navAudioTrack);
            channel_active = dvdnav_get_active_audio_stream(nav);
        }
        // The language code has its first letter in the low byte.
        printf("cDvdPlayer::cDvdPlayer: seen new audio id: 0x%X (%d), <%c%c>; 0x%X (%d)\n", navAudioTrack,
               navAudioTrack, static_cast<char>(audioLanguageCode & 0xFF), static_cast<char>(audioLanguageCode >> 8),
               channel_active, channel_active);
#endif
        navAudioTracksSeen.Add(new IntegerListObject(navAudioTrack));
    }
}

void cDvdPlayer::SetCurrentNavAudioTrackUsrLocked(bool lock) {
    currentNavAudioTrackUsrLocked = lock;
    DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: currentNavAudioTrackUsrLocked=%s\n",
                   currentNavAudioTrackUsrLocked ? "true" : "false");
}

auto cDvdPlayer::GetCurrentNavAudioTrackIdx() const -> int { return SearchAudioStream(currentNavAudioTrack); }

void cDvdPlayer::SetCurrentNavAudioTrackType(int atype) {
    if (atype != currentNavAudioTrackType) {
        DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: atype 0x%X -> 0x%X\n", currentNavAudioTrackType, atype);
    }
    currentNavAudioTrackType = atype;
}

auto cDvdPlayer::GetAudioStreamNumbers() const -> int { return navAudioTracksSeen.Count(); }

void cDvdPlayer::SetAudioTrack(eTrackType Type, const tTrackId *TrackId) {
    if (!TrackId) {
        return;
    }
    // cDevice::SetCurrentAudioTrack() calls this holding its
    // mutexCurrentAudioTrack, which the player thread takes in VDR 2.8.3 with
    // the thread lock held, when it announces or clears tracks
    // (SetAvailableTrack(), ClrAvailableTracks()). Waiting for the thread
    // lock here deadlocked the two when the viewer switched tracks with the
    // Audio key: from another thread, the player thread does the switch.
    if (cThread::ThreadId() != playerThreadId) {
        const cMutexLock MutexLock(&audioTrackRequestMutex);
        audioTrackRequest = Type;
        audioTrackRequestByViewer = cThread::IsMainThread() != 0; // VDR returns it as a tThreadId
        return;
    }
    SwitchAudioTrack(Type, false);
}

void cDvdPlayer::SwitchRequestedAudioTrack() {
    eTrackType type = ttNone;
    bool byViewer = false;
    {
        const cMutexLock MutexLock(&audioTrackRequestMutex);
        type = audioTrackRequest;
        byViewer = audioTrackRequestByViewer;
        audioTrackRequest = ttNone;
    }
    if (type != ttNone) {
        LOCK_THREAD;
        SwitchAudioTrack(type, byViewer);
    }
}

void cDvdPlayer::SwitchAudioTrack(eTrackType Type, bool ByViewer) {
    currentAudioTrack = Type;
    if (!DVDActiveAndRunning()) {
        return;
    }

    // The stream is in the track's number: VDR selects a track before the
    // first packet registers it, with its id still 0, when libdvdnav
    // switches streams at the start of a title. Taken from the id, that was
    // stream 0, which the player passed on while VDR waited for the other.
    int id = (Type - (IS_DOLBY_TRACK(Type) ? ttDolbyFirst : ttAudioFirst)) & AC3AudioTrackMask;
    const int previous = currentNavAudioTrack;

    DEBUG_AUDIO_ID("cDvdPlayer::SetAudioTrack: dd=%d, index=%d, id=0x%2X\n", IS_DOLBY_TRACK(Type), Type, id);

    const int i = SearchAudioStream(id);

    if (i < 0 && navAudioTracksSeen.Count() > 0) {
        id = navAudioTracksSeen.Get(0)->getValue();
        DEBUG_AUDIO_ID("cDvdPlayer::SetAudioTrack: .. not found !\n");
    } else {
        currentNavAudioTrack = -1;
        DEBUG_AUDIO_ID("cDvdPlayer::SetAudioTrack: .. found !\n");
    }

    SetCurrentNavAudioTrackUsrLocked(true);

    if (currentNavAudioTrack != id) {
        currentNavAudioTrack = id;
        const uint16_t audioTrackLangCode = GetAudioTrackLanguageCode(currentNavAudioTrack);

        if (audioTrackLangCode != 0xFFFF) {
            LanguageLetters code = Letters(audioTrackLangCode);
            dvdnav_audio_language_select(nav, code.data());
        }
#if DVDNAV_VERSION >= DVDNAV_VERSION_CODE(7, 0, 0)
        // And the disc's audio stream register, as a player's Audio button
        // sets it. With the language alone the disc took its first stream of
        // that language again at the next title set - Dolby Digital where
        // the viewer had chosen DTS - and its menu showed the old track.
        const int logical = LogicalAudioStream(id);
        if (logical >= 0 && !IsInMenuDomain()) {
            dvdnav_set_active_stream(nav, static_cast<uint8_t>(logical), DVD_AUDIO_STREAM);
        }
#endif
        // The viewer's choice, not VDR passing on the player's own (as VDR's
        // player does it): the new track starts with the picture on screen.
        if (previous != id && ByViewer && !IsInMenuDomain()) {
            if (playMode == ePlayMode::Play) {
                ReplayFromPicture();
            } else if (playMode == ePlayMode::Pause) {
                replayFromPictureOnPlay = true;
            }
        }
        DEBUG_AUDIO_ID("cDvdPlayer::SetAudioTrack: SWITCHED !\n");
    }
    UpdateTrackInfo();
}

void cDvdPlayer::NoteVobu(const pci_t *Pci) {
    uint32_t pos = 0;
    uint32_t len = 0;
    int32_t title = 0;
    int32_t pgcN = 0;
    int32_t pgN = 0;
    if (dvdnav_get_position(nav, &pos, &len) != DVDNAV_STATUS_OK ||
        dvdnav_current_title_program(nav, &title, &pgcN, &pgN) != DVDNAV_STATUS_OK) {
        return;
    }
    cVobuPosition &vobu = vobuHistory.at(static_cast<size_t>(vobuHistoryNext));
    // The player adds ptm_offs to every PTS it passes on.
    vobu.start = Pci->pci_gi.vobu_s_ptm + static_cast<uint32_t>(ptm_offs);
    vobu.end = Pci->pci_gi.vobu_e_ptm + static_cast<uint32_t>(ptm_offs);
    vobu.block = pos;
    vobu.title = title;
    vobu.pgcN = pgcN;
    vobuHistoryNext = (vobuHistoryNext + 1) % VobuHistorySize;
    if (vobuHistoryCount < VobuHistorySize) {
        vobuHistoryCount++;
    }
}

auto cDvdPlayer::ReplayFromPicture() -> bool {
    // Reads on from the VOBU on screen, as VDR's own player does for a new
    // audio track. The device holds a second and more of what was read, so
    // the new track's audio, which starts after that, was that far ahead of
    // the picture: the output device dropped as much video to catch up.
    replayFromPictureOnPlay = false;
    const auto stc = static_cast<int64_t>(DeviceGetSTC());
    int32_t title = 0;
    int32_t pgcN = 0;
    int32_t pgN = 0;
    if (stc < 0 || !nav || dvdnav_current_title_program(nav, &title, &pgcN, &pgN) != DVDNAV_STATUS_OK) {
        return false;
    }
    for (int i = 1; i <= vobuHistoryCount; i++) {
        const cVobuPosition &vobu =
            vobuHistory.at(static_cast<size_t>((vobuHistoryNext - i + VobuHistorySize) % VobuHistorySize));
        // modulo 2^32, as PTS wrap
        if (vobu.title == title && vobu.pgcN == pgcN &&
            static_cast<uint32_t>(stc) - vobu.start < vobu.end - vobu.start) {
            const uint32_t block = vobu.block;
            Empty();
            if (dvdnav_sector_search(nav, block, SEEK_SET) != DVDNAV_STATUS_OK) {
                esyslog("ERROR: dvd-plugin dvd error dvdnav_sector_search: %s", dvdnav_err_to_string(nav));
                return false;
            }
            return true;
        }
    }
    return false;
}

auto cDvdPlayer::DolbyTrackIndex(int SubStreamId) -> int {
    // VDR registers the AC-3 and DTS tracks of the packets it plays itself,
    // numbered by the substream id's low five bits: DTS streams 0x88-0x8f
    // are its Dolby tracks 8-15. With any other number a track is listed
    // twice, once without its language.
    return SubStreamId & AudioTrackMask;
}

auto cDvdPlayer::LogicalAudioStream(int Physical) const -> int {
    // Despite its name, dvdnav_get_audio_logical_stream() maps a logical
    // audio stream - the number the DVD's attributes and menus use - to the
    // physical one in the MPEG stream, so search for the inverse.
    for (int logical = 0; nav && logical < 8; logical++) {
        if (dvdnav_get_audio_logical_stream(nav, static_cast<uint8_t>(logical)) == Physical) {
            return logical;
        }
    }
    return -1;
}

auto cDvdPlayer::LogicalSubpStream(int Physical) const -> int {
    // As for the audio streams, dvdnav_get_spu_logical_stream() maps a logical
    // subpicture stream to its physical one - for a 4:3 picture, or a 16:9 one
    // on a 16:9 TV - so search for the inverse, the current stream first: two
    // logical streams can share a physical one.
    if (!nav || Physical < 0) {
        return -1;
    }
    if (currentNavSubpStream >= 0 &&
        dvdnav_get_spu_logical_stream(nav, currentNavSubpStream & SubpStreamMask) == Physical) {
        return currentNavSubpStream & SubpStreamMask;
    }
    for (int logical = 0; logical < MaxSubpStreams; logical++) {
        if (dvdnav_get_spu_logical_stream(nav, static_cast<uint8_t>(logical)) == Physical) {
            return logical;
        }
    }
    return -1;
}

auto cDvdPlayer::PhysicalSubpStream(int Logical) const -> int {
    // As libdvdnav maps it for a 4:3 picture, or a 16:9 one on a 16:9 TV: the
    // SPU decoder's scale mode stays normal, VDR sets no other.
    if (!nav || Logical < 0) {
        return -1;
    }
    const int physical = dvdnav_get_spu_logical_stream(nav, Logical & SubpStreamMask);
    return physical >= 0 ? physical & SubpStreamMask : -1;
}

auto cDvdPlayer::GetAudioTrackLanguageCode(int Channel) const -> uint16_t {
    uint16_t audioStreamLanguageCode = 0xFFFF;
    // libdvdnav indexes its table of the 8 logical streams' attributes with
    // whatever number it gets, so only ask for a logical stream that exists.
    const int logical = LogicalAudioStream(Channel);
    if (logical >= 0) {
        audioStreamLanguageCode = dvdnav_audio_stream_to_lang(nav, static_cast<uint8_t>(logical));
        audioStreamLanguageCode =
            static_cast<uint16_t>(audioStreamLanguageCode >> 8 | (audioStreamLanguageCode & 0xFF) << 8);
    }
    return audioStreamLanguageCode;
}

namespace {

// The marker of an audio track's code extension, NULL for a plain track.
auto AudioExtension(unsigned int CodeExtension) -> const char * {
    switch (CodeExtension) {
        case 2:
            return tr("DVD$visually impaired");
        case 3: // the director's comments
        case 4: // alternate ones
            return tr("DVD$comments");
        default:
            return nullptr;
    }
}

// The marker of a subtitle stream's code extension, NULL for plain subtitles
// in any size.
auto SubtitleExtension(unsigned int CodeExtension) -> const char * {
    switch (CodeExtension) {
        case 9:
            return tr("DVD$forced");
        case 5: // in the normal size
        case 6: // large
        case 7: // for children
            return tr("DVD$captions");
        case 13: // the director's comments, in the normal size
        case 14: // large
        case 15: // for children
            return tr("DVD$comments");
        default:
            return nullptr;
    }
}

} // namespace

auto cDvdPlayer::AudioTrackDescription(int Channel, const char *LanguageName) const -> cString {
    // VDR's audio menu shows this description in place of the bare language
    // code, so tell the tracks apart by their codec, channel layout and, for
    // commentary or audio-description tracks, a marker - a disc often carries
    // two tracks in the same language that only these distinguish.
    static constexpr std::array<const char *, 8> Codec{"AC3", nullptr, "MPEG", "MPEG", "LPCM", nullptr, "DTS", nullptr};
    const int logical = LogicalAudioStream(Channel);
    audio_attr_t attr;
    if (logical < 0 || dvdnav_get_audio_attr(nav, static_cast<uint8_t>(logical), &attr) != DVDNAV_STATUS_OK) {
        return {LanguageName}; // cString(NULL) keeps whatever VDR already has
    }
    cString d(LanguageName ? LanguageName : "");
    if (const char *c = Codec.at(attr.audio_format & 0x07U)) {
        d = cString::sprintf("%s%s%s", *d, **d ? " " : "", c);
    }
    const int channels = attr.channels + 1; // the field holds the count minus one
    if (channels == 6) {
        d = cString::sprintf("%s%s5.1", *d, **d ? " " : "");
    } else {
        d = cString::sprintf("%s%s%d.0", *d, **d ? " " : "", channels);
    }
    if (const char *ext = AudioExtension(attr.code_extension)) {
        d = cString::sprintf("%s (%s)", *d, ext);
    }
    return **d ? d : cString(LanguageName);
}

auto cDvdPlayer::SubtitleTrackDescription(int Channel, const char *LanguageName) const -> cString {
    // A DVD often carries several subtitle streams in the same language that
    // differ only in their code extension - forced subtitles for foreign
    // dialogue, captions for the hearing impaired, a director's commentary.
    // Mark those, so VDR's subtitle menu tells them apart.
    subp_attr_t attr;
    if (!nav || Channel < 0 || dvdnav_get_spu_attr(nav, Channel & SubpStreamMask, &attr) != DVDNAV_STATUS_OK) {
        return {LanguageName};
    }
    const char *ext = SubtitleExtension(attr.code_extension);
    if (!ext) {
        return {LanguageName};
    }
    return LanguageName ? cString::sprintf("%s (%s)", LanguageName, ext) : cString::sprintf("(%s)", ext);
}

auto cDvdPlayer::GetSubtitleLanguageCode(int Channel) const -> uint16_t {
    // Channel is a logical stream: libdvdnav keeps the attributes by those.
    uint16_t subtitleStreamLanguageCode = 0xFFFF;
    if (nav && Channel >= 0) {
        subtitleStreamLanguageCode = dvdnav_spu_stream_to_lang(nav, Channel & SubpStreamMask);
        subtitleStreamLanguageCode =
            static_cast<uint16_t>(subtitleStreamLanguageCode >> 8 | (subtitleStreamLanguageCode & 0xFF) << 8);
    }
    return subtitleStreamLanguageCode;
}

void cDvdPlayer::ReplaceInfoString(cString &Str, const char *New) {
    const cMutexLock MutexLock(&infoMutex);
    Str = New;
}

auto cDvdPlayer::GetDisplayHeaderLine() const -> cString {
    const cMutexLock MutexLock(&infoMutex);
    return cString::sprintf("%s, %s, %s, %s, %s    ", *titleinfo_str, *infoAudio, *infoSubtitle, *aspect_str,
                            *title_str);
}

void cDvdPlayer::SetTitleString() {
    const char *title_holder = nullptr;

    if (DVDActiveAndRunning() && dvdnav_get_title_string(nav, &title_holder) == DVDNAV_STATUS_OK) {
        ReplaceInfoString(title_str, title_holder);
    } else {
        ReplaceInfoString(title_str, dummy_title);
    }
}

void cDvdPlayer::SetTitleInfoString() {
    if (!DVDActiveAndRunning()) {
        ReplaceInfoString(titleinfo_str, dummy_title);
        return;
    }

    int angleNumber = -1;
    int angleNumbers = -1;
    int chapterNumber = -1;
    int chapterNumbers = -1;
    int titleNumber = -1;
    int titleNumbers = -1;

    dvdnav_get_number_of_titles(nav, &titleNumbers);
    dvdnav_current_title_info(nav, &titleNumber, &chapterNumber);
    dvdnav_get_number_of_parts(nav, titleNumber, &chapterNumbers);

    // A menu is title 0, without chapters and angles.
    if (titleNumber >= 1) {
        dvdnav_get_angle_info(nav, &angleNumber, &angleNumbers);
    }

    cString info;
    if (titleNumber == 0) {
        info = cString::sprintf("%d/%d %d", titleNumber, titleNumbers, chapterNumber);
    } else if (angleNumbers > 1) {
        info = cString::sprintf("%d/%d %d/%d %d/%d", titleNumber, titleNumbers, chapterNumber, chapterNumbers,
                                angleNumber, angleNumbers);
    } else {
        info = cString::sprintf("%d/%d %d/%d", titleNumber, titleNumbers, chapterNumber, chapterNumbers);
    }
    ReplaceInfoString(titleinfo_str, info);
}

void cDvdPlayer::UpdateTrackInfo() {
    cString audio = dummy_n_a;
    cString subtitle = dummy_n_a; // no subtitles, or only the "no subtitles" entry

    if (DVDActiveAndRunning()) {
        const char *audioTypeDescr = nullptr;
        switch (currentNavAudioTrackType) {
            case aAC3:
                audioTypeDescr = "ac3";
                break;
            case aDTS:
                audioTypeDescr = "dts";
                break;
            case aLPCM:
                audioTypeDescr = "pcm";
                break;
            case aMPEG:
                audioTypeDescr = "mp2";
                break;
            default:
                audioTypeDescr = "non";
                break;
        }

        const uint16_t audioStreamLanguageCode = GetAudioTrackLanguageCode(currentNavAudioTrack);
        const LanguageLetters audioLanguage = Letters(audioStreamLanguageCode);
        const char *audioLanguageStr = audioStreamLanguageCode != 0xFFFF ? audioLanguage.data() : "";
        if (GetAudioStreamNumbers() > 1) {
            audio = cString::sprintf("%s %d/%d %s", audioLanguageStr, GetCurrentNavAudioTrackIdx() + 1,
                                     GetAudioStreamNumbers(), audioTypeDescr);
        } else {
            audio = cString::sprintf("%s %s", audioLanguageStr, audioTypeDescr);
        }

        // As in VDR's menu, forced subtitles only count as none.
        const int shownSubpStream = SubtitlesShown(currentNavSubpStream) ? currentNavSubpStream : -1;
        const uint16_t subtitleStreamLanguageCode = GetSubtitleLanguageCode(shownSubpStream);
        const LanguageLetters subtitleLanguage = Letters(subtitleStreamLanguageCode);
        const bool known = subtitleStreamLanguageCode != 0xFFFF;
        if (GetSubtitleStreamNumbers() > 2) {
            subtitle = cString::sprintf("%s %d/%d", known ? subtitleLanguage.data() : "",
                                        SearchSubpStream(shownSubpStream), GetSubtitleStreamNumbers() - 1);
        } else if (GetSubtitleStreamNumbers() > 1) {
            subtitle = known ? subtitleLanguage.data() : dummy_n_a;
        }
    }

    const cMutexLock MutexLock(&infoMutex);
    infoAudio = std::move(audio);
    infoSubtitle = std::move(subtitle);
}

void cDvdPlayer::SetAspectString() {
    const char *aspect = dummy_n_a;

    if (DVDActiveAndRunning()) {
        switch (dvdnav_get_video_aspect(nav)) {
            case 0:
                aspect = " 4:3";
                break;
            case 2:
                aspect = "16:9_";
                break;
            case 3:
                aspect = "16:9";
                break;
            default:
                break;
        }
    }
    ReplaceInfoString(aspect_str, aspect);
}

auto cDvdPlayer::GetReplayMode(bool &Playing, bool &Forwards, int &Speed) -> bool {
    Playing = (playMode == ePlayMode::Play || playMode == ePlayMode::Fast);
    Forwards = (playDir == ePlayDir::Forward);
    if (playMode == ePlayMode::Fast || playMode == ePlayMode::Slow) {
        Speed = Setup.MultiSpeedMode ? abs(trickSpeed - NORMAL_SPEED) : 0;
    } else {
        Speed = -1;
    }
    return true;
}

auto cDvdPlayer::callRootMenu() -> int {
    DEBUGDVD("cDvdPlayer::callRootMenu()\n");

    LOCK_THREAD;
    SetCurrentNavAudioTrackUsrLocked(false);
    return MenuCall(DVD_MENU_Root);
}

auto cDvdPlayer::callTitleMenu() -> int {
    DEBUGDVD("cDvdPlayer::callTitleMenu()\n");

    LOCK_THREAD;
    SetCurrentNavAudioTrackUsrLocked(false);
    return MenuCall(DVD_MENU_Title);
}

auto cDvdPlayer::callSubpMenu() -> int {
    DEBUGDVD("cDvdPlayer::callSubpMenu()\n");

    LOCK_THREAD;
    return MenuCall(DVD_MENU_Subpicture);
}

auto cDvdPlayer::callAudioMenu() -> int {
    DEBUGDVD("cDvdPlayer::callAudioMenu()\n");

    LOCK_THREAD;
    SetCurrentNavAudioTrackUsrLocked(false);
    return MenuCall(DVD_MENU_Audio);
}
