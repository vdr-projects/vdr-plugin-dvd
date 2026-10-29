/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#include <ctype.h>
#include <sys/time.h>
#include <vdr/i18n.h>
#include <vdr/thread.h>
#include <vdr/device.h>
#include <vdr/plugin.h>
#include <vdr/videodir.h>

#include "common-dvd.h"
#include "dvddev.h"
#include "setup-dvd.h"
#include "tools-dvd.h"
#include "player-dvd.h"
#include "control-dvd.h"

// 100 ms each: a device that takes no data for 10 seconds in normal play.
#define POLLTIMEOUTS_BEFORE_DEVICECLEAR 100

#define OsdInUse() ((controller != NULL) ? controller->OsdVisible(this) : false)
#define TakeOsd()  ((controller != NULL) ? controller->TakeOsd((void *)this) : false)

#ifdef CTRLDEBUG

static void printCellInfo(dvdnav_cell_change_event_t &lastCellEventInfo, int64_t pgcTicksPerBlock)
{
    int cell_length_s = (int)(lastCellEventInfo.cell_length / 90000L);
    int pg_length_s = (int)(lastCellEventInfo.pg_length / 90000L);
    int pgc_length_s = (int)(lastCellEventInfo.pgc_length / 90000L);
    int cell_start_s = (int)(lastCellEventInfo.cell_start / 90000L);
    int pg_start_s = (int)(lastCellEventInfo.pg_start / 90000L);

    printf(
        "cellN %ld, pgN=%d, cell_length=%ld %d, pg_length=%ld %d, pgc_length=%ld %d, cell_start %ld %d, pg_start %ld %d, ticksPerBlock=%ld, secPerBlock=%ld\n",
        (long)lastCellEventInfo.cellN, lastCellEventInfo.pgN, (long)lastCellEventInfo.cell_length, cell_length_s,
        (long)lastCellEventInfo.pg_length, pg_length_s, (long)lastCellEventInfo.pgc_length, pgc_length_s,
        (long)lastCellEventInfo.cell_start, cell_start_s, (long)lastCellEventInfo.pg_start, pg_start_s,
        (long)pgcTicksPerBlock, (long)(pgcTicksPerBlock / 90000L));
}

#endif

// --- cIframeAssembler ------------------------------------------------------

class cIframeAssembler {
private:
    int head;
    int size;
    unsigned char *buf;
    cMutex mutex;
    void Lock(void) { mutex.Lock(); }
    void Unlock(void) { mutex.Unlock(); }

public:
    cIframeAssembler(int Size);
    virtual ~cIframeAssembler(void);
    int Available(void) { return head; }
    void Clear(void);
    unsigned char *Get(int &Lenth);
    void Put(unsigned char *Data, int Length);
};

cIframeAssembler::cIframeAssembler(int Size)
{
    size = Size;
    head = 0;
    buf = MALLOC(unsigned char, (size_t)Size);
}

cIframeAssembler::~cIframeAssembler(void)
{
    free(buf);
}

void cIframeAssembler::Clear(void)
{
    Lock();
    head = 0;
    Unlock();
}

unsigned char *cIframeAssembler::Get(int &Length)
{
    Length = head;
    return buf;
}

void cIframeAssembler::Put(unsigned char *Data, int Length)
{
    Lock();
    if (head + Length < size) {
        memcpy(buf + head, Data, (size_t)Length);
        head += Length;
    } else
        esyslog("ERROR: dvd-plugin iframeassembler full %d", head + Length);
    Unlock();
}

// --- cResumeEntry ------------------------------------------------------------

// The resume file and its entries come from the mplayer plugin.
class cResumeEntry : public cListObject {
public:
    char *key;
    int title;
    int chapter;
    int64_t second;
    cResumeEntry(void);
    ~cResumeEntry();
};

cResumeEntry::cResumeEntry(void)
{
    key = 0;
}

cResumeEntry::~cResumeEntry()
{
    free(key);
}

// --- cDVDPlayerResume ----------------------------------------------------------

// The resume file, in VDR's video directory unless RESUMEDIR names another.
#define RESUME_FILE "dvdplayer.resume"
#ifndef RESUMEDIR
#define RESUMEDIR cVideoDirectory::Name()
#endif

// A line per disc: "title:chapter:second:key".
class cDVDPlayerResume : public cList<cResumeEntry> {
private:
    cString resfile;
    bool modified; // the entries in memory differ from the file
    // Replaces the entries in memory with the file's.
    void LoadResume();
    // Writes the entries to the file if they changed; false if that failed.
    bool SaveResume(void);
    // NULL if there is no entry for the key.
    cResumeEntry *FindResume(const char *key);

public:
    cDVDPlayerResume(void);
    ~cDVDPlayerResume();
    // Both read the file first, which keeps the other discs' entries.
    // Stores the position for the key and writes the file.
    void SetResume(const char *key, int title, int chapter, int64_t second);
    // The position stored for the key; false if there is none, and the
    // arguments keep their values.
    bool GetResume(const char *key, int &title, int &chapter, int64_t &second);
};

cDVDPlayerResume::cDVDPlayerResume(void)
{
    modified = false;
    resfile = AddDirectory(RESUMEDIR, RESUME_FILE);
}

cDVDPlayerResume::~cDVDPlayerResume()
{
    SaveResume();
}

void cDVDPlayerResume::SetResume(const char *key, int title, int chapter, int64_t second)
{
    LoadResume();
    cResumeEntry *re = FindResume(key);
    if (re) {
        DEBUGDVD("resume: setting resume %d:%d:%lld (update)", title, chapter, (long long)second);
    } else {
        re = new cResumeEntry;
        re->key = strdup(key);
        Add(re);
        DEBUGDVD("resume: setting resume %d:%d:%lld (new)", title, chapter, (long long)second);
    }
    re->title = title;
    re->chapter = chapter;
    re->second = second;
    modified = true;
    SaveResume();
}

bool cDVDPlayerResume::GetResume(const char *key, int &title, int &chapter, int64_t &second)
{
    LoadResume();
    cResumeEntry *re = FindResume(key);
    if (re) {
        title = re->title;
        chapter = re->chapter;
        second = re->second;
        return true;
    }
    return false;
}

void cDVDPlayerResume::LoadResume()
{
    // Without a file, no entries.
    Clear();
    modified = false;
    DEBUGDVD("resume: resume file is \"%s\"\n", *resfile);
    FILE *f = fopen(resfile, "r");
    if (f) {
        DEBUGDVD("resume: successfully opened resume file\n");
        char line[768];
        while (fgets(line, sizeof(line), f)) {
            char key[512];
            int t, c;
            int64_t s;
            if (sscanf(line, "%d:%d:%" SCNd64 ":%511[^\n]", &t, &c, &s, key) == 4) {
                cResumeEntry *re = new cResumeEntry;
                re->key = strdup(key);
                re->title = t;
                re->chapter = c;
                re->second = s;
                Add(re);
            }
        }
        fclose(f);
    }
}

bool cDVDPlayerResume::SaveResume(void)
{
    if (modified) {
        DEBUGDVD("resume: saving resume file\n");
        cSafeFile f(resfile);
        if (f.Open()) {
            for (cResumeEntry *re = First(); re; re = Next(re))
                fprintf(f, "%d:%d:%" PRId64 ":%s\n", re->title, re->chapter, re->second, re->key);
            f.Close();
            return true;
        } else {
            DEBUGDVD("resume: failed to save resume file\n");
            return false;
        }
    } else {
        return true;
    }
}

cResumeEntry *cDVDPlayerResume::FindResume(const char *key)
{
    DEBUGDVD("resume: searching resume position for \"%s\"\n", key);
    for (cResumeEntry *re = First(); re; re = Next(re)) {
        if (!strcasecmp(re->key, key)) {
            DEBUGDVD("resume: found resume position %d:%d:%lld\n", re->title, re->chapter, (long long)re->second);
            return re;
        }
    }
    DEBUGDVD("resume: no resume position found\n");
    return NULL;
}

// --- cDvdPlayer ------------------------------------------------------------

// The ring buffer of the packets for the device.
#define VIDEOBUFSIZE MEGABYTE(1)

// The speeds as in VDR's player; beyond MAX_SPEEDS, the player skips blocks.
#define MAX_VIDEO_SLOWMOTION 63 // the largest Speed for DeviceTrickSpeed(), as VDR's player passes it
#define NORMAL_SPEED         10 // the index of the '1' entry in the following array
#define MAX_SPEEDS           3  // the offset of the maximum speed from normal speed in either direction
#define MAX_MAX_SPEEDS       (MAX_SPEEDS * MAX_SPEEDS) // the offset of the maximum speed skipping blocks
#define SPEED_MULT           12                        // the speed multiplier
int cDvdPlayer::Speeds[] = {0, 0, 0, 0, 0, 0, 0, -2, -4, -8, 1, 2, 4, 12, 0, 0, 0, 0, 0, 0, 0};

const int cDvdPlayer::MaxAudioTracks = 0x20;
const int cDvdPlayer::AudioTrackMask = 0x1F;
const int cDvdPlayer::AC3AudioTrackMask = 0x07;
const int cDvdPlayer::MaxSubpStreams = 0x20;
const int cDvdPlayer::SubpStreamMask = 0x1F;

const char *cDvdPlayer::dummy_title = "DVD Title";
const char *cDvdPlayer::dummy_n_a = "n.a.";

cDvdPlayer::cDvdPlayer(void) : cThread("dvd-plugin")
{
    DEBUGDVD("cDvdPlayer::cDvdPlayer(void)\n");
    ringBuffer = new cRingBufferFrame(VIDEOBUFSIZE);
    rframe = NULL;
    pframe = NULL;
    controller = NULL;
    active = true;
    running = false;
    playMode = pmPlay;
    playDir = pdForward;
    trickSpeed = NORMAL_SPEED;
    stillTimer = 0;
    nav = 0;
    iframeAssembler = new cIframeAssembler(KILOBYTE(CIF_MAXSIZE));
    IframeCnt = 0;
    event_buf = new uint8_t[4096];
    current_pci = 0;
    prev_e_ptm = 0;
    ptm_offs = 0;
    DEBUG_SUBP_ID("SPU showSubs=%d\n", DVDSetup.ShowSubtitles);

    skipPlayVideo = false;
    videoPictureType = 0;
    subtitleTrackSync = false;
    audioTrackRequest = ttNone;
    audioTrackRequestByViewer = false;
    playerThreadId = 0;
    subpMenuChoice = false;
    vobuHistoryCount = 0;
    vobuHistoryNext = 0;
    replayFromPictureOnPlay = false;
    emptyCount = 0;
    fastWindFactor = 1;

    resume = new cDVDPlayerResume;
    navFailed = false;

    clearSeenSubpStream();
    clearSeenAudioTrack();

    currentNavSubpStream = -1;
    currentNavSubpPhysical = -1;
    subpPhysicalSeen = 0;

    currentNavAudioTrack = -1; // until libdvdnav announces the active stream
    currentNavAudioTrackType = -1;
    cntVidBlocksPlayed = cntAudBlocksPlayed = 0;

    pgcCurrentBlockNum = 1;
    pgcTotalBlockNum = 1;
    pgcTotalTicks = 1;
    pgcTicksPerBlock = 1;
    isInFeature = false;

    dvd_aspect = 0;
    dvd_scaleperm = 0;

    SPUdecoder = NULL;

    titleinfo_str = 0;
    title_str = 0;
    aspect_str = NULL;
    infoPositionValid = false;
    infoCurrentBlock = infoTotalBlock = 0;
    infoPgcLength = 0;
    strcpy(infoAudio, "n.a.");
    strcpy(infoSubtitle, "n.a.");
    menuButtons = false;
    SetTitleInfoString();
    SetTitleString();
    SetAspectString();
    memset(&lastCellEventInfo, 0, sizeof(dvdnav_cell_change_event_t));
}

cDvdPlayer::~cDvdPlayer()
{
    DEBUGDVD("destructor cDvdPlayer::~cDvdPlayer()\n");
    Detach();
    delete[] event_buf;
    delete iframeAssembler;
    delete ringBuffer;
    if (titleinfo_str)
        free(titleinfo_str);

    if (title_str)
        free(title_str);

    if (aspect_str)
        free(aspect_str);

    delete resume;
}

void cDvdPlayer::setController(cDvdPlayerControl *ctrl)
{
    controller = ctrl;
}

bool cDvdPlayer::IsDvdNavigationForced() const
{
    return (controller ? controller->IsDvdNavigationForced() : false);
}

void cDvdPlayer::TrickSpeed(int Increment)
{
    int nts = trickSpeed + Increment;
    cntVidBlocksPlayed = 0;
    cntAudBlocksPlayed = 0;

    if (nts > 0 && nts - NORMAL_SPEED <= MAX_SPEEDS && Speeds[nts] == 1) {
        fastWindFactor = 1;
        trickSpeed = nts;

        if (playMode == pmFast)
            Play();
        else
            Pause();
    } else if (nts > 0 && nts - NORMAL_SPEED <= MAX_SPEEDS && Speeds[nts]) {
        fastWindFactor = 1;
        if (playDir == pdBackward && DVDSetup.ReadAHead > 0)
            dvdnav_set_readahead_flag(nav, 0);
        trickSpeed = nts;
        int Mult = (playMode == pmSlow) ? 1 : SPEED_MULT;
        int sp = (Speeds[nts] > 0) ? Mult / Speeds[nts] : -Speeds[nts] * Mult;

        if (sp > MAX_VIDEO_SLOWMOTION)
            sp = MAX_VIDEO_SLOWMOTION;

        if (playDir == pdBackward) {
            fastWindFactor = (playMode == pmSlow) ? trickSpeed - NORMAL_SPEED : trickSpeed - NORMAL_SPEED + 1;
            if (playMode == pmSlow)
                sp = 2;
        }
        DeviceTrickSpeed(sp, playDir == pdForward);
    } else if (nts > 0 && nts - NORMAL_SPEED <= MAX_MAX_SPEEDS) {
        fastWindFactor = 1;
        trickSpeed = nts;
        if (playDir == pdBackward)
            if (playMode == pmSlow)
                fastWindFactor = trickSpeed - NORMAL_SPEED;
            else
                fastWindFactor = (trickSpeed - NORMAL_SPEED - MAX_SPEEDS + 1) * 2;
        else if (playDir == pdForward && playMode == pmFast)
            fastWindFactor = (trickSpeed - NORMAL_SPEED - MAX_SPEEDS + 1) * 2;
    }

    DEBUG_CONTROL("dvd TRICKSPEED: %d (%d), fastWindFactor=%d fwd=%d, slow=%d, fast=%d\n", trickSpeed, Increment,
                  fastWindFactor, playDir == pdForward, playMode == pmSlow, playMode == pmFast);
}

void cDvdPlayer::DeviceReset(void)
{
    DEBUG_CONTROL("DeviceReset: Clear & Play\n");
    cntVidBlocksPlayed = 0;
    cntAudBlocksPlayed = 0;
    cPlayer::DeviceClear();
    cPlayer::DevicePlay();
}

void cDvdPlayer::DeviceClear(void)
{
    DEBUG_CONTROL("DeviceClear\n");
    if (IsAttached())
        PlayPes(NULL, 0);
    cPlayer::DeviceClear();
}

void cDvdPlayer::DevicePlay(void)
{
    DEBUG_CONTROL("DevicePlay\n");
    cPlayer::DevicePlay();
}

// How often the player thread refreshes the track info the control shows.
#define TRACKINFOINTERVALMS 200

// Consecutive read errors after which the player gives up.
#define MAXREADERRORS 5

#if APIVERSNUM > 30013
// How long the picture may stand still in normal play before
// WaitDeviceDrained() stops waiting: the device holds no more than that
// picture then, or it can't tell that it has played out - VDR's default
// DrainDevice() never does.
#define DRAINSTALLMS 1000
#else
// How long WaitDeviceDrained() waits at most for the device to play out.
#define DRAINTIMEOUTMS 6000
#endif

bool cDvdPlayer::WaitDeviceDrained(void)
{
    // Called with the thread lock held, which is released while the device
    // plays out: that takes as long as the device buffers, with vaapivideo
    // up to nine seconds of a clip without audio, and the control's keys
    // must not wait for it. Returns false if one of them emptied the player
    // or stopped it meanwhile: what the caller held is gone.
    // A fixed limit cut such a clip short.
    int emptied = emptyCount;
#if APIVERSNUM > 30013
    uint64_t stc = DeviceGetSTC();
    cTimeMs stcMoved;
    while (!DeviceDrain()) {
        uint64_t now = DeviceGetSTC();
        // A frozen device plays nothing out until it runs again.
        if (now != stc || playMode != pmPlay) {
            stc = now;
            stcMoved.Set();
        } else if (stcMoved.Elapsed() > DRAINSTALLMS) {
            dsyslog("dvd-plugin: device not drained, its picture stands still");
            break;
        }
        Unlock();
        cCondWait::SleepMs(10);
        Lock();
        if (emptyCount != emptied || !running)
            return false;
    }
#else
    cTimeMs Timeout(DRAINTIMEOUTMS);
    while (!DeviceFlush(100) && !Timeout.TimedOut())
        ;
#endif
    return emptyCount == emptied;
}

void cDvdPlayer::DrawSPU()
{
    LOCK_THREAD;
    if (SPUdecoder && !SPUdecoder->IsVisible() && TakeOsd()) {
        SPUdecoder->Draw();
    }
}

void cDvdPlayer::HideSPU()
{
    LOCK_THREAD;
    if (SPUdecoder) {
        SPUdecoder->Hide();
    }
}

void cDvdPlayer::EmptySPU()
{
    if (SPUdecoder) {
        ClearButtonHighlight();
        SPUdecoder->Empty();
    }
}

void cDvdPlayer::Empty(bool emptyDeviceToo)
{
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
    pframe = NULL;
    ringBuffer->Clear();
    cntVidBlocksPlayed = 0;
    cntAudBlocksPlayed = 0;

    stillTimer = 0;
    stillFrame = 0;
    IframeCnt = 0;
    iframeAssembler->Clear();

    skipPlayVideo = false;
    videoPictureType = 0;

    if (emptyDeviceToo) {
        DeviceClear();
    }
}

void cDvdPlayer::StillSkip()
{
    DEBUGDVD("cDvdPlayer::StillSkip()\n");
    if (stillTimer != 0) {
        stillTimer = 0;
        if (nav != NULL) {
            dvdnav_still_skip(nav);
        }
    }
}

// After a jump of the player's own, libdvdnav keeps the PCI of the last NAV
// packet until it reads the first one of the jump's target. A button of the
// menu left, activated meanwhile, ran in the target's domain: its JumpTT
// from a title failed an assertion in libdvdnav's VM and aborted VDR.
void cDvdPlayer::ForgetButtons(void)
{
    current_pci = NULL;
    menuButtons = false;
}

// Unlike libdvdnav's other jumps, dvdnav_part_play() keeps the still of the
// cell it leaves: out of a still menu, libdvdnav reported that still again
// and again and the title never started. dvdnav_still_skip() would skip the
// still at the end of the target's first cell instead, the jump to the top
// of the program dvdnav_part_play() went to clears it.
bool cDvdPlayer::PartPlay(int Title, int Part)
{
    if (dvdnav_part_play(nav, Title, Part) != DVDNAV_STATUS_OK)
        return false;
    dvdnav_top_pg_search(nav);
    ForgetButtons();
    return true;
}

// dvdnav_menu_call() leaves a still itself. dvdnav_still_skip() before it
// skipped the still of the menu called, which a still menu ends with.
int cDvdPlayer::MenuCall(DVDMenuID_t Menu)
{
    if (!nav || dvdnav_menu_call(nav, Menu) != DVDNAV_STATUS_OK)
        return 0;
    ForgetButtons();
    return 1;
}

#ifdef DVDDEBUG
// The events by number: libdvdnav no longer sends 11, SEEK_DONE.
static const char *dvd_ev[] = {
    "BLOCK_OK",         "NOP",  "STILL_FRAME", "SPU_STREAM_CHANGE", "AUDIO_STREAM_CHANGE", "VTS_CHANGE",  "CELL_CHANGE",
    "NAV_PACKET",       "STOP", "HIGHLIGHT",   "SPU_CLUT_CHANGE",   "SEEK_DONE",           "HOP_CHANNEL", "WAIT",
    "TRICK_MODE_CHANGE"};
#endif

// stillFrame: the cell ends in a still picture, the VOBU is one.
#define CELL_STILL 0x02
#define NAV_STILL  0x04

// Time in ticks of the 90 kHz MPEG clock: 90 a millisecond.
uint64_t cDvdPlayer::time_ticks(void)
{
    // monotonic, so that setting the clock can't end or prolong a still frame
    return cTimeMs::Now() * 90U;
}

void cDvdPlayer::delay_ticks(uint64_t ticks)
{
    uint64_t t0 = time_ticks();
    usleep(1000); // sleeps at least once
    uint64_t done = time_ticks() - t0;
    while (ticks > done) {
        // usleep() oversleeps by up to about 19 ms
        if (ticks - done > 19U * 90U)
            usleep((useconds_t)((ticks - done) * 100U / 9U - 19000U));
        done = time_ticks() - t0;
    }
}

char *cDvdPlayer::GetDVDResumeKey() const
{
    // "DVDName_TotalTitles_OverallChapters". Not unique, but libdvdnav tells
    // neither the disc's serial number nor its size.
    int totalTitles;
    if (dvdnav_get_number_of_titles(nav, &totalTitles)) {
        int totalChapters = 0;
        for (int t = 1; t <= totalTitles; t++) {
            int curChapters;
            dvdnav_get_number_of_parts(nav, t, &curChapters);
            totalChapters += curChapters;
            DEBUGDVD("resume: cDvdPlayer::Action() Title %d has %d chapters.\n", t, curChapters);
        }
        DEBUGDVD("resume: cDvdPlayer::Action() Titles: %d with %d chapters all together, Title: \"%s\"\n", totalTitles,
                 totalChapters, title_str);
        char *key;
        if (asprintf(&key, "%s_%d_%d", title_str, totalTitles, totalChapters) < 0)
            return NULL;
        return key;
    } else {
        return NULL;
    }
}

void cDvdPlayer::SaveResume()
{
    if (resume) {
        int currentTitle, currentChapter;
        // A menu is title 0, which can't be resumed.
        if (dvdnav_current_title_info(nav, &currentTitle, &currentChapter) && (0 != currentTitle)) {
            int64_t currentSec, totalSec;
            GetPositionInSec(currentSec, totalSec);
            char *key = GetDVDResumeKey();
            if (key) {
                DEBUGDVD("resume->SetResume(\"%s\", %d, %d, %lld)\n", key, currentTitle, currentChapter,
                         (long long)currentSec);
                resume->SetResume(key, currentTitle, currentChapter, currentSec);
                free(key);
            } else {
                DEBUGDVD("resume: ERROR computing resume key for this dvd!\n");
            }
        } else {
            DEBUGDVD("resume: ERROR fetching current title and chapter (maybe in menus?).\n");
        }
    }
}

bool cDvdPlayer::LoadResume(int &title, int &chapter, int64_t &second)
{
    bool retval = false;
    if (resume) {
        char *key = GetDVDResumeKey();
        if (key) {
            DEBUGDVD("resume->GetResume(\"%s\", ...): ", key);
            if (resume->GetResume(key, title, chapter, second)) {
                DEBUGDVD("%d:%d:%lld\n", title, chapter, (long long)second);
                // Some seconds back, as VDR resumes a recording.
                int ResumeRewind = 30;
                if (second > ResumeRewind) {
                    second -= ResumeRewind;
                }
                retval = true;
            } else {
                DEBUGDVD("<none>\n");
                retval = false;
            }
            free(key);
        } else {
            DEBUGDVD("resume: ERROR computing resume key for this dvd.\n");
        }
    }
    return retval;
}

void cDvdPlayer::Action(void)
{
    playerThreadId = cThread::ThreadId();
    memset(event_buf, 0, sizeof(uint8_t) * 4096);

    unsigned char *write_blk = NULL;
    int blk_size = 0;

    // libdvdnav writes the handle while it opens the disc, which takes
    // seconds; the control's thread reads nav under the thread lock.
    dvdnav_t *opened = NULL;
#if DVDNAV_VERSION >= DVDNAV_VERSION_CODE(6, 1, 0)
    static const dvdnav_logger_cb navLogger = {NavLog};
    if (dvdnav_open2(&opened, this, &navLogger, cDVD::DeviceName()) != DVDNAV_STATUS_OK) {
#else
    if (dvdnav_open(&opened, const_cast<char *>(cDVD::DeviceName())) != DVDNAV_STATUS_OK) {
#endif
        Skins.QueueMessage(mtError, tr("Error.DVD$Error opening DVD!"));
        esyslog("ERROR: dvd-plugin cannot open dvdnav device %s -> input thread ended (pid=%d) !",
                const_cast<char *>(cDVD::DeviceName()), getpid());
        active = running = false;
        fflush(NULL);
        return;
    }
    {
        LOCK_THREAD;
        nav = opened;
    }
    dvdnav_set_readahead_flag(nav, DVDSetup.ReadAHead);
    if (DVDSetup.PlayerRCE != 0)
        dvdnav_set_region_mask(nav, 1 << (DVDSetup.PlayerRCE - 1));
    else
        dvdnav_set_region_mask(nav, 0xffff);
    dvdnav_menu_language_select(nav, const_cast<char *>(ISO639code[DVDSetup.MenuLanguage]));
    dvdnav_audio_language_select(nav, const_cast<char *>(ISO639code[DVDSetup.AudioLanguage]));
    dvdnav_spu_language_select(nav, const_cast<char *>(ISO639code[DVDSetup.SpuLanguage]));
    DEBUGDVD("Default-Langs: menu=%s, audio=%s, spu=%s\n", const_cast<char *>(ISO639code[DVDSetup.MenuLanguage]),
             const_cast<char *>(ISO639code[DVDSetup.AudioLanguage]),
             const_cast<char *>(ISO639code[DVDSetup.SpuLanguage]));
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
                DEBUG_PTS("\n***** WARNING >=1000ms sleep %llums\n", (unsigned long long)(sleept / 90U));
            }
            delay_ticks(sleept);

            DEBUG_PTS2(
                "dvd loop sleep=%5ut(%3ums), blk_size=%3d, skipPlayV=%d, AudioBlock=%d IframeCnt=%d stillTimer=%llu\n",
                (unsigned int)sleept, (unsigned int)sleept / 90U, blk_size, skipPlayVideo, playedPacket == pktAudio,
                IframeCnt, (unsigned long long)stillTimer / 90U);
        }
        sleept = 0;
        SwitchRequestedAudioTrack(); // also while paused
        if (playMode == pmPause || playMode == pmStill) {
            cCondWait::SleepMs(10);
            continue;
        }

        cPoller Poller;
        if (!DevicePoll(Poller, 100)) {
            // Only in normal play: in slow motion a device takes data at a
            // fraction of the normal rate, and after a jump or a trick mode
            // it may not take any while it plays out what it holds.
            if (playMode == pmPlay && ++PollTimeouts == POLLTIMEOUTS_BEFORE_DEVICECLEAR) {
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
        if (!running)
            continue; // a key handler found libdvdnav broken, see NavLog()

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

        trickMode = playMode == pmFast || (playMode == pmSlow && playDir == pdBackward);

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
                    if (!WaitDeviceDrained())
                        continue; // pframe is gone
                    if (!firstClear)
                        DeviceReset();
                }

                // The video of a still picture goes to the device as its
                // assembled I-frame, below.
                res = (IframeCnt > 0 && frameType == ftVideo) ? blk_size : PlayPes(write_blk, blk_size);

                if (trickMode) {
                    DEBUG_CONTROL("PLAYED  : size=%d, written=%d\n", blk_size, res);
                }
            }
#ifdef CTRLDEBUG
            else if (trickMode)
                printf("SKIPPED : size=%d\n", blk_size);
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
                pframe = NULL;
            }
            continue;
        } else {
            if (playedPacket == pktAudio) {
                cntAudBlocksPlayed++;
            }
            playedPacket = pktNone;
            frameType = ftUnknown;
        }

        if (IframeCnt > 0) {
            // Only if the device played video: in a menu with audio only the
            // reset would cut the sound.
            if (cntVidBlocksPlayed > 0) {
                DEBUG_CONTROL("clearing device because of IframeCnt > 0, vid %d, aud %d\n", cntVidBlocksPlayed,
                              cntAudBlocksPlayed);
                if (!WaitDeviceDrained())
                    continue; // the I-frame is gone
                DeviceReset();
            }
            int iframeSize;
            unsigned char *iframe = iframeAssembler->Get(iframeSize);

            DEBUG_IFRAME("I-Frame: DeviceStillPicture: IframeCnt=%d->-1, iframe=%d, used=%d;\n", IframeCnt,
                         iframe != NULL, iframeSize);

            if (iframe && iframeSize > 0) {
#ifdef IFRAMEWRITE
                static int ifnum = 0;
                static char ifname[255];
                snprintf(ifname, 255, "/tmp/dvd.iframe.%3.3d.asm.pes", ifnum++);
                FILE *f = fopen(ifname, "wb");
                if (f) {
                    fwrite(iframe, 1, (size_t)iframeSize, f);
                    fclose(f);
                }
#endif
                DeviceStillPicture(iframe, iframeSize);

                DEBUG_IFRAME("SEND; ");
                if (!WaitDeviceDrained())
                    continue; // emptied, which also ended the still
                DEBUG_IFRAME("FLUSH!\n");
            }
            iframeAssembler->Clear();
            IframeCnt = -1; // a still picture was played
            if (blk_size <= 0 && !skipPlayVideo)
                sleept = 1 * 90U; // 1 ms

            DEBUG_IFRAME("I-Frame: DeviceStillPicture: vpts=%8ums sleept=%llums\n", (unsigned int)(VideoPts / 90U),
                         (unsigned long long)(sleept / 90U));
            continue;
        }

        // Fast forward beyond the device's speeds: skip ahead.
        if (playDir == pdForward && playMode == pmFast && skipPlayVideo && cntVidBlocksPlayed > 0 &&
            fastWindFactor > 1) {
            uint32_t pos, len;
            int64_t pgcPosTicks = 0, pgcPosTicksIncr = 0;

            dvdnav_set_PGC_positioning_flag(nav, 1);
            dvdnav_get_position(nav, &pos, &len);

            pgcPosTicks = (int64_t)pos * pgcTicksPerBlock;

            // fastWindFactor half seconds ahead, not past the end
            pgcPosTicksIncr = (int64_t)((fastWindFactor * 90000L) / 2L);

            // pgcTicksPerBlock is 0 if the program chain's playback time is
            // shorter than its number of blocks, or 0 itself.
            if (pgcPosTicks + pgcPosTicksIncr < pgcTotalTicks && pgcTicksPerBlock > 0) {
                DEBUG_CONTROL("dvd %d %4.4u fwd get block: %4.4ldb %10.10ldt %lds\n", playDir == pdBackward,
                              cntVidBlocksPlayed, (long)pos, (long)pgcPosTicks, (long)(pgcPosTicks / 90000L));

                DEBUG_CONTROL(
                    "dvd fwd set block: ts=%d, factor=%ds, %ldms+%ldms=%ldms, %4.4ldb+%4.4ldb=%4.4ldb, %10.10ldt+%10.10ldt=%10.10ldt\n",
                    trickSpeed, fastWindFactor / 2, (long)(pgcPosTicks / 90L), (long)(pgcPosTicksIncr / 90L),
                    (long)((pgcPosTicks + pgcPosTicksIncr) / 90L), (long)pos,
                    (long)(pgcPosTicksIncr / pgcTicksPerBlock),
                    (long)((pgcPosTicks + pgcPosTicksIncr) / pgcTicksPerBlock), (long)pgcPosTicks,
                    (long)pgcPosTicksIncr, (long)pgcPosTicks + (long)pgcPosTicksIncr);

                pos = (uint32_t)((pgcPosTicks + pgcPosTicksIncr) / pgcTicksPerBlock);

                if (dvdnav_sector_search(nav, pos, SEEK_SET) != DVDNAV_STATUS_OK)
                    esyslog("ERROR: dvd-plugin dvd error dvdnav_sector_search: %s", dvdnav_err_to_string(nav));
            }
            cntVidBlocksPlayed = 0;
        }

        // Backward, fast or slow: jump back.
        else if (playDir == pdBackward && skipPlayVideo && cntVidBlocksPlayed > 0 && fastWindFactor != 1) {
            uint32_t pos = 0, posdiff = 0, len = 0;

            if (fastWindFactor < 0) {
                // slow motion
                if (slomoloop == 0) {
                    slomoloop = abs(fastWindFactor);
                    cntVidBlocksPlayed++; // one more block back ..
                    posdiff = (uint32_t)(cntVidBlocksPlayed * 2);
                    DEBUG_CONTROL("dvd slomo jump: d%ldb %dsl (%df)\n", (long)posdiff, slomoloop, fastWindFactor);
                } else {
                    posdiff = (uint32_t)(cntVidBlocksPlayed);
                    DEBUG_CONTROL("dvd slomo loop: d%ldb %dsl (%df)\n", (long)posdiff, slomoloop, fastWindFactor);
                    slomoloop--;
                }
            } else {
                // Up to MAX_SPEEDS back by the blocks played times
                // fastWindFactor, beyond by fastWindFactor half seconds.
                if (trickSpeed - NORMAL_SPEED <= MAX_SPEEDS || pgcTicksPerBlock <= 0) {
                    cntVidBlocksPlayed++; // one more block back ..
                    posdiff = cntVidBlocksPlayed * (uint32_t)fastWindFactor;
                } else {
                    int64_t pgcPosTicksDecr = 0;
                    pgcPosTicksDecr = (int64_t)((fastWindFactor * 90000L) / 2L);
                    posdiff = (uint32_t)(pgcPosTicksDecr / pgcTicksPerBlock);
                }
            }

            // unless that is before the beginning
            dvdnav_set_PGC_positioning_flag(nav, 1);
            if (dvdnav_get_position(nav, &pos, &len) == DVDNAV_STATUS_OK && pos > posdiff) {
                uint32_t forcedBlockPosition = pos - posdiff;
                DEBUG_CONTROL("dvd bwd set block: ts=%d, factor=%d, %ld-%ld=%ld, slo=%d\n", trickSpeed, fastWindFactor,
                              (long)pos, (long)posdiff, (long)forcedBlockPosition, slomoloop);
                if (dvdnav_sector_search(nav, forcedBlockPosition, SEEK_SET) != DVDNAV_STATUS_OK)
                    esyslog("ERROR: dvd-plugin dvd error dvdnav_sector_search: %s", dvdnav_err_to_string(nav));
            } else {
                DEBUG_CONTROL("dvd %d %4.4u bwd get block: %4.4ldb d%4.4ldb, slo=%d\n", playDir == pdBackward,
                              cntVidBlocksPlayed, (long)pos, (long)posdiff, slomoloop);
            }
            cntVidBlocksPlayed = 0;
        } else if (playMode == pmFast || playMode == pmSlow) {
            uint32_t pos, len;
            int64_t pgcPosTicks = 0;

            dvdnav_set_PGC_positioning_flag(nav, 1);
            dvdnav_get_position(nav, &pos, &len);

            pgcPosTicks = (int64_t)pos * pgcTicksPerBlock;

            DEBUG_CONTROL("dvd %d %4.4u any get block: %4.4ldb %10.10ldt %lds, sec=%d\n", playDir == pdBackward,
                          cntVidBlocksPlayed, (long)pos, (long)pgcPosTicks, (long)(pgcPosTicks / 90000L),
                          (int)(pgcPosTicks / 90000L));

            // At the last block go back 10 s instead of running past the end.
            if (playDir == pdForward && pos + 1 == len && pgcPosTicks > 90000L * 10L && pgcTicksPerBlock > 0) {
                pgcPosTicks -= 90000L * 10L;
                if (dvdnav_sector_search(nav, pgcPosTicks / pgcTicksPerBlock, SEEK_SET) != DVDNAV_STATUS_OK)
                    esyslog("ERROR: dvd-plugin dvd error dvdnav_sector_search: %s", dvdnav_err_to_string(nav));
            }
        }

        uint8_t *cache_ptr = event_buf;
        int32_t event;
        int32_t len;

        // From here on no continue: it would bypass dvdnav_free_cache_block().

        dvdnav_status_t navStatus = dvdnav_get_next_cache_block(nav, &cache_ptr, &event, &len);
        if (navFailed) {
            // the event comes from a VM that can't be asked anything anymore
            if (navStatus == DVDNAV_STATUS_OK && cache_ptr != 0 && cache_ptr != event_buf)
                dvdnav_free_cache_block(nav, cache_ptr);
            break;
        }
        if (navStatus != DVDNAV_STATUS_OK) {
            esyslog("ERROR: dvd-plugin %s", dvdnav_err_to_string(nav));
            if (ReadErrors++ == 0)
                Skins.QueueMessage(mtError, tr("Error.DVD$Error fetching data from DVD!"));
            // The reset starts over at the beginning of the disc. If even
            // that can't be read (the disc has been taken out), stop.
            if (ReadErrors >= MAXREADERRORS) {
                esyslog("ERROR: dvd-plugin giving up after %d consecutive read errors", ReadErrors);
                running = false;
            } else
                dvdnav_reset(nav);
            continue;
        }
        // Only data read from the disc counts as recovered: after a reset
        // libdvdnav reports the new title set and cell from the IFO files.
        if (event == DVDNAV_BLOCK_OK || event == DVDNAV_NAV_PACKET)
            ReadErrors = 0;
        noAudio = playMode != pmPlay;

        switch (event) {
            case DVDNAV_BLOCK_OK:
                UpdateBlockInfo();
                playedPacket = playPacket(cache_ptr, trickMode, noAudio);
                break;
            case DVDNAV_NOP:
                DEBUG_NAV("%s:%d:NAV NOP\n", __FILE__, __LINE__);
                break;
            case DVDNAV_STILL_FRAME: {
                uint64_t currentTicks = time_ticks();
                DEBUG_PTS2("%s:%d:NAV STILL FRAME (menu=%d), rem. stillTimer=%llut(%llums), currTicks=%llut(%llums)\n",
                           __FILE__, __LINE__, IsInMenuDomain(), (unsigned long long)stillTimer,
                           (unsigned long long)stillTimer / 90, (unsigned long long)currentTicks,
                           (unsigned long long)currentTicks / 90);
                dvdnav_still_event_t *still = (dvdnav_still_event_t *)cache_ptr;
                if (stillTimer != 0) {
                    seenVPTS(0);
                    if (stillTimer > currentTicks) {
                        sleept = 40 * 90U; // 40 ms
                    } else {
                        StillSkip();
                        sleept = 0;
                        DEBUG_PTS("Still time clear\n");
                    }
                    DEBUG_PTS2("StillTimer->Sleep: %llut(%llums)\n", (unsigned long long)sleept,
                               (unsigned long long)sleept / 90);
                } else {
                    if (still->length == 0xff) {
                        stillTimer = UINT64_MAX; // until the user leaves it
                        DEBUG_PTS("Still time (max)\n");
                    } else {
                        stillTimer = currentTicks + (uint64_t)(still->length) * 90000U;
                        DEBUG_PTS("Still time (set): %llut(%llums)\n", (unsigned long long)stillTimer,
                                  (unsigned long long)stillTimer / 90);
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
                dvdnav_spu_stream_change_event_t *ev = (dvdnav_spu_stream_change_event_t *)cache_ptr;
                DEBUG_SUBP_ID("SPU Streams: w:0x%X (%d), l:0x%X (%d), p:0x%X (%d), locked=%d\n", ev->physical_wide,
                              ev->physical_wide, ev->physical_letterbox, ev->physical_letterbox, ev->physical_pan_scan,
                              ev->physical_pan_scan, currentNavSubpStreamUsrLocked);

                if (IsInMenuDomain() || IsDvdNavigationForced() || !currentNavSubpStreamUsrLocked) {
                    int previousNavSubpStream = currentNavSubpStream;
                    cSpuDecoder::eScaleMode mode = SPUdecoder ? SPUdecoder->getScaleMode() : cSpuDecoder::eSpuNormal;
                    // libdvdnav gives the physical streams of the active logical
                    // one, -1 for none; bit 7 set means hide, and only let forced
                    // subtitles show (see vm.c from libdvdnav). The logical
                    // number it leaves out.
                    int physical = mode == cSpuDecoder::eSpuLetterBox ? ev->physical_letterbox : // TV 4:3, DVD 16:9
                                       mode == cSpuDecoder::eSpuPanAndScan ? ev->physical_pan_scan
                                                                           : ev->physical_wide;
                    currentNavSubpPhysical = physical < 0 ? -1 : physical & SubpStreamMask;
                    currentNavSubpStream = -1;
                    if (ev->physical_wide >= 0) {
                        currentNavSubpStream = LogicalSubpStream(ev->physical_wide & SubpStreamMask);
                        if (currentNavSubpStream < 0) // a menu's
                            currentNavSubpStream = ev->physical_wide & SubpStreamMask;
                        currentNavSubpStream |= ev->physical_wide & 0x80;
                    }
                    DEBUG_SUBP_ID("dvd choosing SPU stream: curNavSpu=0x%X, physical %d (mode %d)\n",
                                  currentNavSubpStream, currentNavSubpPhysical, mode);
                    // A menu's subpicture stream holds its buttons, not subtitles.
                    if (currentNavSubpStream != previousNavSubpStream && !IsInMenuDomain())
                        ReportSubtitleTrack();
                } else {
                    DEBUG_SUBP_ID("DVDNAV_SPU_STREAM_CHANGE: ignore (locked=%d|not enabled=%d), menu=%d, feature=%d \n",
                                  currentNavSubpStreamUsrLocked, DVDSetup.ShowSubtitles, IsInMenuDomain(), isInFeature);
                }
                break;
            }
            case DVDNAV_AUDIO_STREAM_CHANGE: {
                DEBUG_NAV("%s:%d:NAV AUDIO STREAM CHANGE\n", __FILE__, __LINE__);
                if (!currentNavAudioTrackUsrLocked) {
                    int id = dvdnav_get_active_audio_stream(nav);
                    if (id >= 0) {
                        // Only Dolby Digital and DTS are Dolby tracks for VDR; it
                        // plays MPEG audio and LPCM as ttAudio, the track type has
                        // to match or VDR drops the stream.
                        int logical = LogicalAudioStream(id);
                        uint16_t format = logical >= 0 ? dvdnav_audio_stream_format(nav, (uint8_t)logical)
                                                       : (uint16_t)DVD_AUDIO_FORMAT_AC3;
                        bool dolby = format == DVD_AUDIO_FORMAT_AC3 || format == DVD_AUDIO_FORMAT_DTS;
                        DEBUG_AUDIO_ID("dvd->SetCurrentAudioTrack %s %02X\n", dolby ? "DOLBY" : "AUDIO", id);
                        eTrackType track;
                        if (dolby) {
                            int subStreamId = (format == DVD_AUDIO_FORMAT_DTS ? aDTS : aAC3) | id;
                            track = eTrackType(ttDolby + DolbyTrackIndex(subStreamId));
                        } else
                            track = eTrackType(ttAudio + id);
                        // libdvdnav announces the active stream at every cell.
                        // VDR switches to a Dolby track again even if it is the
                        // current one, and the device can only reset its audio
                        // for that: with several Dolby tracks it can't tell
                        // which one comes, so every cell cut a second of sound.
                        if (track != currentAudioTrack)
                            DeviceSetCurrentAudioTrack(track);
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
                if (playMode == pmPlay && (cntVidBlocksPlayed > 0 || cntAudBlocksPlayed > 0))
                    WaitDeviceDrained();
                if (cntVidBlocksPlayed > 0)
                    firstClear = false;
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
                    int resTitle, resChapter;
                    if (LoadResume(resTitle, resChapter, resSecond))
                        next_cell_change = GotoTitle(resTitle, resChapter) > 0;
                }
                break;
            case DVDNAV_CELL_CHANGE: {
                DEBUG_NAV("%s:%d:NAV CELL CHANGE\n", __FILE__, __LINE__);
                if (cntVidBlocksPlayed > 0)
                    firstClear = false;

                memcpy(&lastCellEventInfo, cache_ptr, sizeof(dvdnav_cell_change_event_t));
                UpdateBlockInfo();
                UpdateVTSInfo();
                BlocksToPGCTicks(1, pgcTicksPerBlock, pgcTotalTicks);
#ifdef CTRLDEBUG
                printCellInfo(lastCellEventInfo, pgcTicksPerBlock);
#endif
                // The cell that ended may have ended in a still picture.
                SendIframe(stillFrame & CELL_STILL);
                stillFrame = (dvdnav_get_next_still_flag(nav) != 0) ? CELL_STILL : 0;
                SetTitleInfoString();
                if (next_cell_change) {
                    next_cell_change = false;
                    Goto((int)resSecond);
                }
                break;
            }
            case DVDNAV_NAV_PACKET: {
                DEBUG_NAV0("%s:%d:NAV PACKET\n", __FILE__, __LINE__);
                dsi_t *dsi;
                current_pci = dvdnav_get_current_nav_pci(nav);
                menuButtons = current_pci && current_pci->hli.hl_gi.hli_ss;
                DEBUG_NAV0("NAV: %x, prev_e_ptm: %8d, s_ptm: %8d, ", stillFrame, prev_e_ptm,
                           current_pci->pci_gi.vobu_s_ptm);
                if (prev_e_ptm)
                    ptm_offs += prev_e_ptm - (int)current_pci->pci_gi.vobu_s_ptm;
                DEBUG_NAV0("ptm_offs: %8d\n", ptm_offs);
                prev_e_ptm = (int)current_pci->pci_gi.vobu_e_ptm;
                if (!IsInMenuDomain())
                    NoteVobu(current_pci);
                if (current_pci && (current_pci->hli.hl_gi.hli_ss & 0x03) == 1)
                    UpdateButtonHighlight(NULL);
                dsi = dvdnav_get_current_nav_dsi(nav);
                SendIframe(stillFrame & NAV_STILL);
                if (dsi->vobu_sri.next_video == 0xbfffffff)
                    stillFrame |= NAV_STILL;
                else
                    stillFrame &= (uint8_t)~NAV_STILL;
                break;
            }
            case DVDNAV_STOP:
                DEBUG_NAV("%s:%d:NAV STOP\n", __FILE__, __LINE__);
                running = false;
                break;
            case DVDNAV_HIGHLIGHT: {
                DEBUG_NAV("%s:%d:NAV HIGHLIGHT\n", __FILE__, __LINE__);
                dvdnav_highlight_event_t *hlevt = (dvdnav_highlight_event_t *)cache_ptr;
                UpdateButtonHighlight(hlevt);
                break;
            }
            case DVDNAV_SPU_CLUT_CHANGE:
                DEBUG_NAV("%s:%d:NAV SPU CLUT CHANGE SPUdecoder=%d\n", __FILE__, __LINE__, SPUdecoder != NULL);
                if (SPUdecoder) {
                    ClearButtonHighlight();
                    SPUdecoder->setPalette((uint32_t *)cache_ptr);
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
        if ((event != DVDNAV_BLOCK_OK) && (event != DVDNAV_NAV_PACKET) && (event != DVDNAV_STILL_FRAME))
            DEBUGDVD("got event (%d)%s, len %d\n", event, dvd_ev[event], len);
#endif
        if (cache_ptr != 0 && cache_ptr != event_buf)
            dvdnav_free_cache_block(nav, cache_ptr);
        if (trackInfoTimeout.TimedOut()) {
            UpdateTrackInfo();
            trackInfoTimeout.Set(TRACKINFOINTERVALMS);
        }
    }

    DEBUG_NAV("%s:%d: empty\n", __FILE__, __LINE__);

    if (navFailed)
        Skins.QueueMessage(mtError, tr("Error.DVD$Error fetching data from DVD!"));
    Empty();
    fflush(NULL);

    running = false;

    {
        // The control's thread uses nav and SPUdecoder under this lock.
        LOCK_THREAD;
        SPUdecoder = NULL;
        menuButtons = false;
        current_pci = NULL; // points into nav
        dvdnav_close(nav);
        nav = NULL;
        UpdateTrackInfo();
        cMutexLock MutexLock(&infoMutex);
        infoPositionValid = false;
    }

    DEBUGDVD("%s:%d: input thread ended (pid=%d)\n", __FILE__, __LINE__, getpid());
    fflush(NULL);

    // Last: DVDRemoveable() now tells the control to end the replay.
    active = false;
}

#if DVDNAV_VERSION >= DVDNAV_VERSION_CODE(6, 1, 0)
void cDvdPlayer::NavLog(void *Player, dvdnav_logger_level_t Level, const char *Format, va_list Args)
{
    char msg[256];
    vsnprintf(msg, sizeof(msg), Format, Args);
    cDvdPlayer *player = static_cast<cDvdPlayer *>(Player);
    // libdvdnav repeats its notes: on a disc without menus in the menu
    // language every jump to a menu brought "Language 'de' not found, using
    // 'en' instead" and the disc's menu languages, hundreds of lines in an
    // evening. Log a note once a replay, an error each time.
    if (Level != DVDNAV_LOGGER_LEVEL_ERROR && !player->NavLogFirst(msg))
        return;
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

bool cDvdPlayer::NavLogFirst(const char *Msg)
{
    cMutexLock MutexLock(&navLogMutex);
    if (navLogged.Find(Msg) >= 0)
        return false;
    // A bound for a disc with endless different notes, which are logged then.
    if (navLogged.Size() < 100)
        navLogged.Append(strdup(Msg));
    return true;
}
#endif

void cDvdPlayer::ClearButtonHighlight(void)
{
    LOCK_THREAD;

    if (SPUdecoder) {
        SPUdecoder->clearHighlight();
        DEBUG_NAV("DVD NAV SPU clear button\n");
    }
}

void cDvdPlayer::activateButton(void)
{
    DEBUGDVD("activateButton\n");
    LOCK_THREAD;
    if (nav && current_pci) {
        int buttonN;
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

void cDvdPlayer::UpdateButtonHighlight(dvdnav_highlight_event_t *hlevt)
{
    LOCK_THREAD;

    if (hlevt) {
        DEBUG_NAV("DVD NAV SPU highlight evt: %d: %d/%d %d/%d (%dx%d)\n", hlevt->buttonN, hlevt->sx, hlevt->sy,
                  hlevt->ex, hlevt->ey, hlevt->ex - hlevt->sx + 1, hlevt->ey - hlevt->sy + 1);
    } else {
        DEBUG_NAV("DVD NAV SPU highlight evt: NULL\n");
    }

    int buttonN = -1;

    dvdnav_get_current_highlight(nav, &buttonN);

    ClearButtonHighlight();

    if (SPUdecoder && current_pci && TakeOsd()) {
        dvdnav_highlight_area_t hl;

        // libdvdnav ignores the button groups, which letterbox and pan & scan
        // need for the right highlight.
        bool libdvd_workaround = false;

        if (current_pci->hli.hl_gi.btngr_ns) {
            int btns_per_group = 36 / current_pci->hli.hl_gi.btngr_ns;
            btni_t *button_ptr = NULL;
            int modeMask = 1 << SPUdecoder->getScaleMode();

            if (!button_ptr && current_pci->hli.hl_gi.btngr_ns >= 1 &&
                (current_pci->hli.hl_gi.btngr1_dsp_ty & modeMask))
                button_ptr = &current_pci->hli.btnit[0 * btns_per_group + buttonN - 1];
            if (!button_ptr && current_pci->hli.hl_gi.btngr_ns >= 2 &&
                (current_pci->hli.hl_gi.btngr2_dsp_ty & modeMask))
                button_ptr = &current_pci->hli.btnit[1 * btns_per_group + buttonN - 1];
            if (!button_ptr && current_pci->hli.hl_gi.btngr_ns >= 3 &&
                (current_pci->hli.hl_gi.btngr3_dsp_ty & modeMask))
                button_ptr = &current_pci->hli.btnit[2 * btns_per_group + buttonN - 1];

            if (button_ptr && button_ptr->btn_coln != 0) {
                int sx = button_ptr->x_start;
                int ex = button_ptr->x_end;
                if ((ex - sx + 1) & 0x03)
                    ex += 4 - ((ex - sx + 1) & 0x03);

                SPUdecoder->setHighlight((uint16_t)sx, button_ptr->y_start, (uint16_t)ex, button_ptr->y_end,
                                         current_pci->hli.btn_colit.btn_coli[button_ptr->btn_coln - 1][0]);
                libdvd_workaround = true;
            }
        }

        if (!libdvd_workaround && dvdnav_get_highlight_area(current_pci, buttonN, 0, &hl) == DVDNAV_STATUS_OK) {
            DEBUG_NAV("DVD NAV SPU highlight button: %d: %d/%d %d/%d (%dx%d)\n", buttonN, hl.sx, hl.sy, hl.ex, hl.ey,
                      hl.ex - hl.sx + 1, hl.ey - hl.sy + 1);

            if ((hl.ex - hl.sx + 1) & 0x03) {
                hl.ex = (uint16_t)(hl.ex + 4 - ((hl.ex - hl.sx + 1) & 0x03));
            }
            DEBUG_NAV("\t\t-> %d/%d %d/%d (%dx%d)\n", hl.sx, hl.sy, hl.ex, hl.ey, hl.ex - hl.sx + 1, hl.ey - hl.sy + 1);

            SPUdecoder->setHighlight(hl.sx, hl.sy, hl.ex, hl.ey, hl.palette);

            if (pktpts != 0xFFFFFFFF)
                seenVPTS(pktpts);
        } else {
            // no button selected yet: the first
            if (buttonN == 0) {
                DEBUG_NAV("DVD NAV SPU select button 1 %s:%d\n", __FILE__, __LINE__);
                dvdnav_button_select(nav, current_pci, 1);
            }
        }
    } else {
        DEBUG_NAV("not current pci button: %d, SPUdecoder=%d, current_pci=%p\n", buttonN, SPUdecoder != NULL,
                  (void *)current_pci);
    }
}

// The aspect ratio of a sequence header, as the DVD allows it on the TV:
// 1 = square pixels, 2 = 4:3, 3 = 16:9, 4 = 2.21:1.
void cDvdPlayer::DoScaleMode(int &vaspect)
{
    // Only for a DVD that is not 4:3, on a 4:3 TV.
    if (!Setup.VideoFormat && dvd_aspect != 0) {
        if (vaspect != 2) {
            if (!(dvd_scaleperm & 1)) // letterbox allowed: keep 16:9
                vaspect = 0x03;
            else if (!(dvd_scaleperm & 2)) { // pan & scan allowed
                vaspect = 0x02;              // 4:3
            }
        }
        if (vaspect == 2 && dvd_aspect == 3) // letterbox, as the DVD's aspect ratio says
            vaspect = 0x03;                  // 16:9
    }
}

void cDvdPlayer::seenVPTS(uint64_t pts)
{
    seenSpuPts(pts);
}

void cDvdPlayer::seenAPTS(uint64_t pts)
{
    seenSpuPts(pts);
}

void cDvdPlayer::seenSpuPts(uint64_t pts)
{
    LOCK_THREAD;
    if (SPUdecoder && TakeOsd()) {
        SPUdecoder->setTime((uint32_t)pts);
    }
}

void cDvdPlayer::SendIframe(bool doSend)
{
    if (IframeCnt == 0 && iframeAssembler->Available() && doSend) {
        DEBUG_IFRAME("I-Frame: Doing StillFrame: IframeCnt=%d->1, used=%d, doSend=%d, stillFrame=0x%X\n", IframeCnt,
                     iframeAssembler->Available(), doSend, stillFrame);
        IframeCnt = 1;
    }
}

void cDvdPlayer::playSPU(int spuId)
{
    int spuSize = SPUassembler.getSize();
    uint8_t *buffer = new uint8_t[size_t(spuSize)];
    SPUassembler.Get(buffer, SPUassembler.getSize());
    // Without allowedShow, the SPU decoder shows the forced subpictures only,
    // such as a menu's buttons.
    bool allowedShow = IsDvdNavigationForced() || SubtitlesShown(spuId);

    DEBUG_SUBP_ID("playSPU: SPU proc, spu_size:%d, menuDomain=%d, pts: %12llu\n", spuSize, IsInMenuDomain(),
                  (unsigned long long)SPUassembler.getPts());

    SPUdecoder->processSPU((uint32_t)SPUassembler.getPts(), buffer, allowedShow);
    if (IsInMenuDomain())
        seenVPTS(pktpts); // in a title the video's PTS follow
}

// The ISO 639-1 code a DVD gives for a track's language in Language, empty
// if there is none, and the language's name, or NULL.
static const char *TrackLanguage(uint16_t LanguageCode, char *Language)
{
    Language[0] = char(LanguageCode & 0xFF);
    Language[1] = char(LanguageCode >> 8);
    Language[2] = 0;
    if (LanguageCode == 0xFFFF || !isalpha((unsigned char)Language[0]) || !isalpha((unsigned char)Language[1])) {
        Language[0] = 0;
        return NULL;
    }
    for (int i = 0; i < DvdLanguageCodes; i++) {
        if (!strcasecmp(Language, DvdLanguageCode[i][0]))
            return DvdLanguageCode[i][1];
    }
    return NULL;
}

void cDvdPlayer::SetAvailableAudioTrack(eTrackType Type, int Index, uint16_t Id)
{
    // DVDs give a track's language as an ISO 639-1 code. That is the track's
    // language, which only holds a few bytes and is what VDR's audio menu
    // shows; the language's name goes into the description. The stream's
    // number is the low bits of its id, also where Index is a DTS track's.
    uint16_t languageCode = GetAudioTrackLanguageCode(Id & AC3AudioTrackMask);
    // This runs for every audio packet, and VDR's audio menu reads a track's
    // language and description without a lock: write them only once.
    eTrackType track = eTrackType(Type + Index);
    if (track > ttNone && track < ttMaxTrackTypes) {
        if (announcedLanguageCode[track] == languageCode) {
            DeviceSetAvailableTrack(Type, Index, Id);
            return;
        }
        announcedLanguageCode[track] = languageCode;
    }
    char language[3];
    const char *languageName = TrackLanguage(languageCode, language);
    cString trackDescription = AudioTrackDescription(Id & AC3AudioTrackMask, languageName);
    DeviceSetAvailableTrack(Type, Index, Id, language[0] ? language : NULL, trackDescription);
}

void cDvdPlayer::AnnounceSubtitleTrack(int Stream)
{
    // VDR's subtitle menu lists the subtitle tracks; this player draws the
    // subtitles itself, VDR never gets their packets.
    char language[3];
    const char *languageName = TrackLanguage(GetSubtitleLanguageCode(Stream), language);
    cString trackDescription = SubtitleTrackDescription(Stream, languageName);
    subtitleTrackSync = true;
    DeviceSetAvailableTrack(ttSubtitle, Stream, (uint16_t)(0x20 | Stream), language[0] ? language : NULL,
                            trackDescription);
    subtitleTrackSync = false;
}

bool cDvdPlayer::SubtitlesShown(int Stream) const
{
    // Whether all of Stream's subtitles are shown, not just the forced ones:
    // the viewer chose it with key 2 or VDR's menu, or the disc shows it (bit
    // 7 clear) and that may count. "Display subtitles: no" hides what a disc
    // switches on by itself - not what the viewer chose in the disc's menu,
    // where the disc shows the selected stream the same way.
    if (Stream < 0)
        return false;
    if (currentNavSubpStreamUsrLocked)
        return true;
    return !(Stream & 0x80) && (DVDSetup.ShowSubtitles || subpMenuChoice);
}

void cDvdPlayer::ReportSubtitleTrack(void)
{
    // As a manual choice, so that VDR doesn't pick a track of its own when
    // one is announced: the disc or the user chose this one. A stream that
    // shows forced subtitles only is what VDR's menu calls "none".
    int stream = currentNavSubpStream;
    eTrackType type = SubtitlesShown(stream) ? eTrackType(ttSubtitleFirst + (stream & SubpStreamMask)) : ttNone;
    subtitleTrackSync = true;
    cDevice::PrimaryDevice()->SetCurrentSubtitleTrack(type, true);
    subtitleTrackSync = false;
}

void cDvdPlayer::SetSubtitleTrack(eTrackType Type, const tTrackId *)
{
    LOCK_THREAD; // also orders the flag with the player thread setting it
    // While the player announces or reports a track, VDR's call back only
    // echoes the player's own state, or VDR's guess at one.
    if (subtitleTrackSync || !DVDActiveAndRunning())
        return;
    SetSubpStream(IS_SUBTITLE_TRACK(Type) ? Type - ttSubtitleFirst : -1);
}

int cDvdPlayer::playPacket(unsigned char *&cache_buf, bool trickMode, bool noAudio)
{
    int playedPacket = pktNone;
    unsigned char *sector = cache_buf;

    // Every block of a VOB is a pack and starts with the pack header. A
    // search for a start code further in parsed the rest of the block from
    // there as if it were all of it, that many bytes past its end.
    if (!cPStream::packetStart(sector) || cPStream::packetType(sector) != 0xBA) {
        esyslog("ERROR: dvd-plugin got unexpected packet: %x %x %x %x", sector[0], sector[1], sector[2], sector[3]);
        return playedPacket;
    }

    int offset = 14 + cPStream::stuffingLength(sector);
    sector += offset;
    int r = DVD_VIDEO_LB_LEN - offset;

    sector[6] &= 0x8f;

    uchar *data = sector;
    int datalen = r;

    // MPEG video and audio streams have a range of ids each.
    int streamType = cPStream::packetType(sector);
    if (VIDEO_STREAM_S <= streamType && streamType <= VIDEO_STREAM_E)
        streamType = VIDEO_STREAM_S;
    else if (AUDIO_STREAM_S <= streamType && streamType <= AUDIO_STREAM_E)
        streamType = AUDIO_STREAM_S;

    switch (streamType) {
        case VIDEO_STREAM_S: {
            bool ptsFlag = ((sector[7] & 0x80) && sector[8] >= 5);
            if (ptsFlag) {
                pktpts = cPStream::fromPTS(sector + 9) + (uint64_t)ptm_offs;
            }

            // skip the optional and the mandatory header bytes
            data += cPStream::PESHeaderLength(sector);
            data += 3;
            // What is left of the packet from data on, which starts 6 bytes
            // before the payload: the scan below must not run past it. The
            // length without the header data counted 3 bytes of the next block,
            // and a sequence header starting at the end of a packet had its
            // aspect ratio "corrected" there, in libdvdnav's read cache.
            datalen = min(cPStream::packetLength(sector), r) - int(data - sector);

            // Trick modes play I-frames only, from their start: not the rest of
            // a picture that was under way when the trick mode began.
            if (trickMode && videoPictureType != I_FRAME)
                skipPlayVideo = true;
            uchar *payload = sector + 9 + cPStream::PESHeaderLength(sector);
            uchar *firstHeader = NULL; // of the picture that starts in this packet
            // Bytes to blank in the copy that is played (the block itself is
            // in libdvdnav's read cache), as offsets into the packet.
            int blankFrom = 0, blankTo = 0;

            uint8_t currentFrameType = 0;
            bool do_copy = (lastFrameType == I_FRAME) && !(data[0] == 0 && data[1] == 0 && data[2] == 1);
            bool havePictureHeader = false;
            bool haveSliceBeforePicture = false;
            while (datalen >= 6) { // a picture header's type is in its 6th byte
                if (data[0] == 0 && data[1] == 0 && data[2] == 1) {
                    int ptype2 = cPStream::packetType(data);
                    if (!firstHeader && (ptype2 == SEQUENCE_HEADER || ptype2 == GOP_START || ptype2 == SC_PICTURE))
                        firstHeader = data;
                    if (ptype2 == SEQUENCE_HEADER && trickMode && skipPlayVideo) {
                        // A DVD has a sequence header in front of every GOP,
                        // which starts with an I-frame: play from that header on,
                        // after a clear the decoder can't decode the I-frame
                        // without it.
                        blankFrom = int(payload - sector);
                        blankTo = int(data - sector);
                        skipPlayVideo = false;
                        videoPictureType = I_FRAME;
                    }
                    if (ptype2 == SC_PICTURE) {
                        uchar foundFrameType = (uchar)(data[5] >> 3) & 0x07;
                        if (foundFrameType < I_FRAME || B_FRAME < foundFrameType) {
                            data++;
                            datalen--;
                            continue;
                        }
                        havePictureHeader = true;
                        VideoPts += 3600;
                        lastFrameType = foundFrameType;
                        videoPictureType = foundFrameType;
                        if (!currentFrameType)
                            currentFrameType = lastFrameType;
                        // Trick modes play I-frames only.
                        bool skip = lastFrameType > I_FRAME && trickMode;
                        if (skip && !skipPlayVideo) {
                            // The I-frame being played ends in this packet, its
                            // last slices before this picture: play them, blank
                            // the rest (zero bytes are stuffing before a start
                            // code); the next packet is skipped. Skipping this
                            // one cut the bottom rows off every I-frame.
                            blankFrom = int(data - sector);
                            blankTo = cPStream::packetLength(sector);
                            break;
                        }
                        if (!skip && skipPlayVideo && firstHeader > payload) {
                            // The next I-frame starts after the end of a
                            // skipped picture, which the decoder can't use.
                            blankFrom = int(payload - sector);
                            blankTo = int(firstHeader - sector);
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
                        int aspect = (int)(data[3] & 0xf0) >> 4;

                        DoScaleMode(aspect);

                        data[3] &= 0x0f;
                        data[3] |= (uchar)((aspect & 0x0f) << 4);

                        data += 3;
                        datalen -= 7;
                    } else if (ptype2 == PADDING_STREAM) {
                        DEBUGDVD("PADDING_STREAM @ %d\n", (int)(data - sector));
                        break;
                    } else if (0x01 <= ptype2 && ptype2 <= 0xaf) {
                        if (!havePictureHeader && iframeAssembler->Available() == 0)
                            haveSliceBeforePicture = true;
                    }
                }
                data++;
                datalen--;
            }

            if (stillFrame && (currentFrameType <= I_FRAME || do_copy)) {
                if (haveSliceBeforePicture) {
                    DEBUG_IFRAME2("I-Frame: clr  (%d,%d,c:%d,p:%d,x:%d,v:%u) !\n", currentFrameType, lastFrameType,
                                  (int)do_copy, (int)havePictureHeader, (int)haveSliceBeforePicture,
                                  cntVidBlocksPlayed);
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
            if (blankTo > blankFrom && blankTo <= r)
                memset(rframe->Data() + blankFrom, 0, size_t(blankTo - blankFrom));

            playedPacket |= pktVideo;
            if (!IsInMenuDomain())
                seenVPTS(VideoPts);
            break;
        }
        case AUDIO_STREAM_S: {
            lastFrameType = AUDIO_STREAM_S;
            int audioVal = cPStream::packetType(sector);
            int audioId = audioVal & AudioTrackMask;
            notifySeenAudioTrack(audioId);

            SetAvailableAudioTrack(ttAudio, audioId, (uint16_t)audioVal);

            // no sound in trick mode
            if (noAudio)
                return playedPacket;

            if (currentNavAudioTrack != audioId)
                return playedPacket;

            bool ptsFlag = ((sector[7] & 0xC0) == 0x80) && sector[8] >= 5;
            if (ptsFlag) {
                pktpts = cPStream::fromPTS(sector + 9) + (uint64_t)ptm_offs;
            }

            playedPacket |= pktAudio;
            SetCurrentNavAudioTrackType(aMPEG);

            if (ptsFlag)
                cPStream::toPTS(sector + 9, pktpts);

            rframe = new cFrame(sector, r, ftAudio);

            if (ptsFlag)
                seenAPTS(pktpts);

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
            if (noAudio)
                return playedPacket;

            // skip the packet's start code, stream id and length
            data += 6;

            bool ptsFlag = (sector[7] & 0xC0) && sector[8] >= 5;
            if (ptsFlag) {
                pktpts = cPStream::fromPTS(sector + 9) + (uint64_t)ptm_offs;
            }

            // data now points to the beginning of the payload
            int subStreamId = ((int)*data);
            int audioType = subStreamId & 0xF8;

            if (audioType == aAC3 || audioType == aDTS || audioType == aLPCM) {
                int audioId = subStreamId & AC3AudioTrackMask;

                notifySeenAudioTrack(audioId);

                if (audioType == aLPCM)
                    SetAvailableAudioTrack(ttAudio, audioId, (uint16_t)subStreamId);
                else
                    SetAvailableAudioTrack(ttDolby, DolbyTrackIndex(subStreamId), (uint16_t)subStreamId);

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
                                      (unsigned int)(ptsFlag ? pktpts / 90U : 0), (unsigned int)(VideoPts / 90U), r);
                    if (ptsFlag)
                        seenAPTS(pktpts);
                }
            } else if ((subStreamId & 0xE0) == 0x20) {
                int subtitleIndex = subStreamId & SubpStreamMask;
                // A title's stream that the list lacks: the stream number in
                // the packet is a physical one.
                if (isInFeature && !(subpPhysicalSeen & (1u << subtitleIndex))) {
                    subpPhysicalSeen |= 1u << subtitleIndex;
                    int logical = LogicalSubpStream(subtitleIndex);
                    if (logical >= 0)
                        notifySeenSubpStream(logical);
                }

                if (OsdInUse()) // another OSD is open
                    break;

                data++;
                datalen -= 10; // 3 (mandatory header) + 6 (PS header)

                // The physical stream selects the packets, the logical one
                // tells playSPU() with bit 7 whether only forced subtitles show.
                if (currentNavSubpPhysical != -1 && subtitleIndex == currentNavSubpPhysical) {
                    SPUassembler.Put(data, datalen, pktpts);
                    if (SPUdecoder && SPUassembler.ready())
                        playSPU(currentNavSubpStream);
                }
            } else {
                DEBUGDVD("PRIVATE_STREAM2 unhandled (a)id: %d 0x%X\n", (int)(*data), (int)(*data));
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
    if (rframe && !ringBuffer->Put(rframe))
        delete rframe;
    rframe = NULL;

    return playedPacket;
}

void cDvdPlayer::Activate(bool On)
{
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

void cDvdPlayer::Pause(void)
{
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return;
    if (playMode == pmPause || playMode == pmStill)
        Play();
    else {
        if (playMode == pmFast || (playMode == pmSlow && playDir == pdBackward)) {
            DEBUG_NAV("%s:%d: empty\n", __FILE__, __LINE__);
            Empty();
        }
        DeviceFreeze();
        playMode = pmPause;
    }
}

void cDvdPlayer::Stop(void)
{
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return;

    if (running && nav) {
        SaveResume();
        dvdnav_stop(nav);
        // Stop() runs twice when the viewer stops: in ProcessKey() and in the
        // control's destructor. The second one returns above.
        running = false;
    }
}

void cDvdPlayer::Play(void)
{
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return;
    if (playMode != pmPlay) {
        if (playMode == pmStill || playMode == pmFast || (playMode == pmSlow && playDir == pdBackward)) {
            if (DVDSetup.ReadAHead > 0)
                dvdnav_set_readahead_flag(nav, DVDSetup.ReadAHead);
            DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
            Empty();
        } else if (replayFromPictureOnPlay)
            ReplayFromPicture(); // an audio track chosen while paused
        DevicePlay();
        playMode = pmPlay;
        playDir = pdForward;
    }
}

void cDvdPlayer::Forward(void)
{
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return;
    switch (playMode.load()) {
        case pmFast:
            if (Setup.MultiSpeedMode) {
                TrickSpeed(playDir == pdForward ? 1 : -1);
                break;
            } else if (playDir == pdForward) {
                Play();
                break;
            }
            // fall through
        case pmPlay:
            DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
            Empty();
            DeviceMute();
            playMode = pmFast;
            playDir = pdForward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? 1 : MAX_SPEEDS);
            break;
        case pmSlow:
            if (Setup.MultiSpeedMode) {
                TrickSpeed(playDir == pdForward ? -1 : 1);
                break;
            } else if (playDir == pdForward) {
                Pause();
                break;
            }
            // fall through
        case pmStill:
        case pmPause:
            DeviceMute();
            playMode = pmSlow;
            playDir = pdForward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? -1 : -MAX_SPEEDS);
            break;
    }
}

void cDvdPlayer::Backward(void)
{
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return;
    switch (playMode.load()) {
        case pmFast:
            if (Setup.MultiSpeedMode) {
                TrickSpeed(playDir == pdBackward ? 1 : -1);
                break;
            } else if (playDir == pdBackward) {
                Play();
                break;
            }
            // fall through
        case pmPlay:
            DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
            Empty();
            DeviceMute();
            playMode = pmFast;
            playDir = pdBackward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? 1 : MAX_SPEEDS);
            break;
        case pmSlow:
            if (Setup.MultiSpeedMode) {
                TrickSpeed(playDir == pdBackward ? -1 : 1);
                break;
            } else if (playDir == pdBackward) {
                Pause();
                break;
            }
            // fall through
        case pmStill:
        case pmPause:
            DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
            Empty();
            DeviceMute();
            playMode = pmSlow;
            playDir = pdBackward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? -1 : -MAX_SPEEDS);
            break;
    }
}

inline int64_t cDvdPlayer::GetPGCLengthInTicks(void)
{
    return lastCellEventInfo.pgc_length;
}

void cDvdPlayer::BlocksToPGCTicks(uint32_t BlockNum, int64_t &Ticks, int64_t &TotalTicks)
{
    TotalTicks = GetPGCLengthInTicks();
    if (pgcTotalBlockNum > 0)
        Ticks = BlockNum * (TotalTicks / pgcTotalBlockNum);
    else
        Ticks = 0;
}

void cDvdPlayer::PGCTicksToBlocks(int64_t Ticks, int64_t &BlockNum, uint32_t &TotalBlockNum)
{
    int64_t totalTicks = GetPGCLengthInTicks();
    TotalBlockNum = pgcTotalBlockNum;
    BlockNum = totalTicks > 0 ? (Ticks * TotalBlockNum / totalTicks) : 0;
}

bool cDvdPlayer::GetIndex(int &CurrentFrame, int &TotalFrame, bool)
{
    // The position the player thread read last: the control polls this from
    // VDR's main loop, which must not wait for the thread's lock.
    cMutexLock MutexLock(&infoMutex);
    if (!infoPositionValid || infoTotalBlock == 0)
        return false;
    CurrentFrame = (int)(double(infoCurrentBlock * infoPgcLength / infoTotalBlock / 90000L) * FramesPerSecond());
    TotalFrame = (int)(double(infoPgcLength / 90000L) * FramesPerSecond());
    return true;
}

bool cDvdPlayer::UpdateBlockInfo()
{
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
    cMutexLock MutexLock(&infoMutex);
    infoPositionValid = ok;
    infoCurrentBlock = pgcCurrentBlockNum;
    infoTotalBlock = pgcTotalBlockNum;
    infoPgcLength = GetPGCLengthInTicks();
    return ok;
}

void cDvdPlayer::UpdateVTSInfo()
{
    if (nav) {
        dvd_aspect = dvdnav_get_video_aspect(nav);
        dvd_scaleperm = dvdnav_get_video_scale_permission(nav);
        isInFeature = dvdnav_is_domain_vts(nav);

#if APIVERSNUM >= 30012 // cSpuDecoder::setResolution(), VDR 2.8.1
        uint32_t dvd_width, dvd_height;
        dvdnav_get_video_resolution(nav, &dvd_width, &dvd_height);
        if (SPUdecoder)
            SPUdecoder->setResolution((int)dvd_width, (int)dvd_height);
#endif

        eVideoDisplayFormat videoDisplayformat = vdfCenterCutOut;
        if (!(dvd_scaleperm & 1))
            videoDisplayformat = vdfLetterBox;
        else if (!(dvd_scaleperm & 2))
            videoDisplayformat = vdfPanAndScan;
        DeviceSetVideoDisplayFormat(videoDisplayformat);
    }
}

void cDvdPlayer::SkipSeconds(int Seconds)
{
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return;
    if (Seconds) {
        // In 64 bits: the time search skips up to 99:59 hours. A skip of
        // more than the whole program chain ends at its start or end.
        int64_t totalTicks = GetPGCLengthInTicks();
        int64_t diffTicks = max(min(int64_t(Seconds) * 90000, totalTicks), -totalTicks);
        int64_t diffBlockNum;
        uint32_t totalBlockNum;
        PGCTicksToBlocks(diffTicks, diffBlockNum, totalBlockNum);
        PlayFromBlock(pgcCurrentBlockNum + diffBlockNum);
    }
}

void cDvdPlayer::PlayFromBlock(int64_t Block)
{
    // As VDR's player does it: from the start of the program chain at the
    // earliest, and a position past its end is ignored. A block number that
    // wrapped around made the seek fail, after the player had been emptied.
    if (Block >= pgcTotalBlockNum)
        return;
    DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
    Empty();
    if (dvdnav_sector_search(nav, max(Block, int64_t(0)), SEEK_SET) != DVDNAV_STATUS_OK)
        esyslog("ERROR: dvd-plugin dvd error dvdnav_sector_search: %s", dvdnav_err_to_string(nav));
    Play();
}

bool cDvdPlayer::GetPositionInSec(int64_t &CurrentSec, int64_t &TotalSec)
{
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

void cDvdPlayer::GetPositionInTicks(int64_t &CurrentTicks, int64_t &TotalTicks)
{
    BlocksToPGCTicks(pgcCurrentBlockNum, CurrentTicks, TotalTicks);
}

void cDvdPlayer::Goto(int Seconds)
{
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return;

    // The time search asks for 10 s before the end, which is before the
    // start of a shorter program chain.
    int64_t blockNum;
    uint32_t totalBlockNum;
    PGCTicksToBlocks(min(int64_t(max(Seconds, 0)) * 90000, GetPGCLengthInTicks()), blockNum, totalBlockNum);
    PlayFromBlock(blockNum);
}

int cDvdPlayer::GotoAngle(int Angle)
{
    int angleNumber = -1;
    int angleNumbers = -1;

    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return -1;

    dvdnav_get_angle_info(nav, &angleNumber, &angleNumbers);

    if (Angle > angleNumbers)
        Angle = 1;
    if (Angle <= 0)
        Angle = angleNumbers;

    // Key 3 in a title with a single angle emptied the player, which cut the
    // picture and the sound with no angle to change to.
    if (Angle == angleNumber)
        return Angle;

    DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
    Empty();

    dvdnav_angle_change(nav, Angle);

    Play();

    return Angle;
}

void cDvdPlayer::NextAngle(void)
{
    int angleNumber = -1;
    int angleNumbers = -1;

    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return;
    dvdnav_get_angle_info(nav, &angleNumber, &angleNumbers);
    GotoAngle(++angleNumber);
}

int cDvdPlayer::GotoTitle(int Title, int Chapter)
{
    int titleNumbers;
    int chapterNumber;
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return -1;

    // A title the disc doesn't have - a typo in the title search, a resume
    // entry of another disc - is ignored; NextTitle() and PreviousTitle()
    // wrap around themselves.
    if (dvdnav_get_number_of_titles(nav, &titleNumbers) != DVDNAV_STATUS_OK || Title < 1 || Title > titleNumbers)
        return -1;

    DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
    Empty();

    // A chapter out of range wraps around.
    dvdnav_get_number_of_parts(nav, Title, &chapterNumber);
    if (Chapter > chapterNumber)
        Chapter = 1;
    if (Chapter <= 0)
        Chapter = chapterNumber;

    PartPlay(Title, Chapter);

    Play();

    return Title;
}

// The current title, 0 in a menu. Until libdvdnav has opened a title set it
// can't tell, in the menu of a disc that starts there: the title number was
// left unset, and Yellow and Green went to a title the stack chose, or none.
int cDvdPlayer::CurrentTitle(void)
{
    int title, part;
    return dvdnav_current_title_info(nav, &title, &part) == DVDNAV_STATUS_OK ? title : 0;
}

void cDvdPlayer::NextTitle(void)
{
    int titleNumbers = 0;
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return;
    int titleNumber = CurrentTitle();
    dvdnav_get_number_of_titles(nav, &titleNumbers);
    GotoTitle(titleNumber < titleNumbers ? titleNumber + 1 : 1);
}

void cDvdPlayer::PreviousTitle()
{
    int titleNumbers = 0;
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return;
    int titleNumber = CurrentTitle();
    dvdnav_get_number_of_titles(nav, &titleNumbers);
    GotoTitle(titleNumber > 1 ? titleNumber - 1 : titleNumbers);
}

int cDvdPlayer::GotoPart(int Part)
{
    int titleNumber;
    int chapterNumber;
    int chapterNumbers;
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return -1;

    // A menu is title 0, which has no chapters: libdvdnav left
    // chapterNumbers unset.
    if (dvdnav_current_title_info(nav, &titleNumber, &chapterNumber) != DVDNAV_STATUS_OK || titleNumber < 1 ||
        dvdnav_get_number_of_parts(nav, titleNumber, &chapterNumbers) != DVDNAV_STATUS_OK)
        return -1;

    DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
    Empty();

    if (Part > chapterNumbers)
        Part = 1;
    if (Part <= 0)
        Part = chapterNumbers;

    PartPlay(titleNumber, Part);
    Play();
    return Part;
}

void cDvdPlayer::NextPart(void)
{
    int titleNumber, chapterNumber;
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return;
    if (dvdnav_current_title_info(nav, &titleNumber, &chapterNumber) == DVDNAV_STATUS_OK)
        GotoPart(chapterNumber + 1);
}

void cDvdPlayer::PreviousPart(void)
{
    int titleNumber, chapterNumber;
    LOCK_THREAD;
    if (!DVDActiveAndRunning())
        return;
    if (dvdnav_current_title_info(nav, &titleNumber, &chapterNumber) == DVDNAV_STATUS_OK)
        GotoPart(chapterNumber - 1);
}

void cDvdPlayer::clearSeenSubpStream()
{
    LOCK_THREAD;

    navSubpStreamSeen.Clear();
    currentNavSubpStream = -1;
    currentNavSubpPhysical = -1;
    subpPhysicalSeen = 0;
    notifySeenSubpStream(-1); // the "off" entry
    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: seen subp cleared\n");
    SetCurrentNavSubpStreamUsrLocked(false);
}

void cDvdPlayer::setAllSubpStreams(void)
{
    clearSeenSubpStream();
    ReportSubtitleTrack();

    // The logical streams of the program chain, with a language or without.
    // Not in a menu, whose subpicture stream holds its buttons: outside a
    // title, libdvdnav gives every logical stream the menu's.
    for (int i = 0; nav != NULL && isInFeature && i < MaxSubpStreams; i++) {
        if (dvdnav_get_spu_logical_stream(nav, (uint8_t)i) >= 0)
            notifySeenSubpStream(i);
    }
}

// The index of the stream in the list, bit 7 ignored; -1 if it isn't there.
int cDvdPlayer::SearchSubpStream(int SubpStreamId) const
{
    if (SubpStreamId != -1)
        SubpStreamId &= SubpStreamMask;
    int i = navSubpStreamSeen.Count() - 1;
    while (i >= 0) {
        // the list holds stream numbers without bit 7, and -1
        if (SubpStreamId == navSubpStreamSeen.Get(i)->getValue())
            break; // found
        i--;
    }
    return i;
}

void cDvdPlayer::notifySeenSubpStream(int navSubpStream)
{
    int i = SearchSubpStream(navSubpStream);

    if (i < 0) {
        // only a stream the program chain has
        if (navSubpStream == -1 || (nav && dvdnav_get_spu_logical_stream(nav, (uint8_t)navSubpStream) >= 0)) {
            // The subtitle key steps through this list: keep it in stream
            // order, "off" (-1) first, not in the order the first packets of
            // the streams turned up, which made the key start anywhere.
            IntegerListObject *before = navSubpStreamSeen.First();
            while (before && before->getValue() < navSubpStream)
                before = navSubpStreamSeen.Next(before);
            if (before)
                navSubpStreamSeen.Ins(new IntegerListObject(navSubpStream), before);
            else
                navSubpStreamSeen.Add(new IntegerListObject(navSubpStream));
            if (navSubpStream >= 0)
                AnnounceSubtitleTrack(navSubpStream);
        }
    }
}

void cDvdPlayer::SetCurrentNavSubpStreamUsrLocked(bool lock)
{
    currentNavSubpStreamUsrLocked = lock;
    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: currentNavSubpStreamUsrLocked=%s\n",
                  currentNavSubpStreamUsrLocked ? "true" : "false");
}

int cDvdPlayer::GetCurrentNavSubpStream(void) const
{
    return currentNavSubpStream;
}

int cDvdPlayer::GetSubtitleStreamNumbers(void) const
{
    return navSubpStreamSeen.Count();
}

void cDvdPlayer::SelectSubpStream(void)
{
    // The viewer chose currentNavSubpStream: show its physical stream, and
    // tell libdvdnav the language for the titles to come.
    currentNavSubpPhysical = PhysicalSubpStream(currentNavSubpStream);
    uint16_t subtitleLanguageCode = GetSubtitleLanguageCode(currentNavSubpStream);
    if (subtitleLanguageCode != 0xFFFF && nav)
        dvdnav_spu_language_select(nav, (char *)&subtitleLanguageCode);
#if DVDNAV_VERSION >= DVDNAV_VERSION_CODE(7, 0, 0)
    // And the disc's subpicture stream register, as a player's subtitle button
    // sets it and as for the audio track: with the language alone the disc
    // took its first stream of that language again at the next title set.
    // Whether the disc shows the stream or only its forced subtitles stays.
    if (currentNavSubpStream >= 0 && isInFeature && nav)
        dvdnav_set_active_stream(nav, currentNavSubpStream & SubpStreamMask, DVD_SUBTITLE_STREAM);
#endif
}

void cDvdPlayer::SetSubpStream(int id)
{
    LOCK_THREAD;

    int i = SearchSubpStream(id);

    if (i < 0)
        id = ((IntegerListObject *)navSubpStreamSeen.Get(0))->getValue();

    currentNavSubpStream = id;
    SetCurrentNavSubpStreamUsrLocked(true);
    SelectSubpStream();
    UpdateTrackInfo();

    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: curNavSpu next to 0x%X, idx=%d, physical %d, locked=%d\n",
                  currentNavSubpStream, i, currentNavSubpPhysical, currentNavSubpStreamUsrLocked);
}

void cDvdPlayer::NextSubpStream()
{
    if (navSubpStreamSeen.Count() == 0)
        return;

    LOCK_THREAD;

    int i = SearchSubpStream(currentNavSubpStream);

    i = (i + 1) % navSubpStreamSeen.Count();

    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: found curNavSubp(0x%X) at idx = %d\n", currentNavSubpStream, i);

    currentNavSubpStream = ((IntegerListObject *)navSubpStreamSeen.Get(i))->getValue();
    SetCurrentNavSubpStreamUsrLocked(true);
    SelectSubpStream();
    UpdateTrackInfo();
    ReportSubtitleTrack();
}

void cDvdPlayer::clearSeenAudioTrack()
{
    navAudioTracksSeen.Clear();
    DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: seen audio cleared\n");
    SetCurrentNavAudioTrackUsrLocked(false);
    // This rebuilds the track list on a title or cell change, so VDR must drop
    // the old tracks here even though SetKeepTracks(true) otherwise keeps them.
    cDevice::PrimaryDevice()->SetKeepTracks(false);
    DeviceClrAvailableTracks();
    cDevice::PrimaryDevice()->SetKeepTracks(true);
    currentAudioTrack = ttNone; // VDR's clear resets it
    for (int i = 0; i < ttMaxTrackTypes; i++)
        announcedLanguageCode[i] = -1;
}

void cDvdPlayer::setAllAudioTracks(void)
{
    clearSeenAudioTrack();

    for (int i = 0; nav != NULL && i < MaxAudioTracks; i++) {
        if (GetAudioTrackLanguageCode(i) != 0xFFFF) {
            notifySeenAudioTrack(i);
        }
    }
}

int cDvdPlayer::SearchAudioStream(int AudioStreamId) const
{
    int i = navAudioTracksSeen.Count() - 1;
    while (i >= 0) {
        if (AudioStreamId == ((IntegerListObject *)navAudioTracksSeen.Get(i))->getValue())
            break; // found
        i--;
    }
    return i;
}

void cDvdPlayer::notifySeenAudioTrack(int navAudioTrack)
{
    if (SearchAudioStream(navAudioTrack) < 0) {
#ifdef AUDIOIDDEBUG
        uint16_t audioLanguageCode = 0xFFFF;
        int channel, channel_active = 0;

        channel = navAudioTrack;
        if (nav != NULL) {
            audioLanguageCode = GetAudioTrackLanguageCode(channel);
            channel_active = dvdnav_get_active_audio_stream(nav);
        }
        printf("cDvdPlayer::cDvdPlayer: seen new audio id: 0x%X (%d), <%s>; 0x%X (%d)\n", channel, channel,
               (char *)&audioLanguageCode, channel_active, channel_active);
        fflush(NULL);
#endif
        navAudioTracksSeen.Add(new IntegerListObject(navAudioTrack));
    }
}

void cDvdPlayer::SetCurrentNavAudioTrackUsrLocked(bool lock)
{
    currentNavAudioTrackUsrLocked = lock;
    DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: currentNavAudioTrackUsrLocked=%s\n",
                   currentNavAudioTrackUsrLocked ? "true" : "false");
}

int cDvdPlayer::GetCurrentNavAudioTrackIdx(void) const
{
    return SearchAudioStream(currentNavAudioTrack);
}

void cDvdPlayer::SetCurrentNavAudioTrackType(int atype)
{
    if (atype != currentNavAudioTrackType) {
        DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: atype 0x%X -> 0x%X\n", currentNavAudioTrackType, atype);
    }
    currentNavAudioTrackType = atype;
}

int cDvdPlayer::GetAudioStreamNumbers(void) const
{
    return navAudioTracksSeen.Count();
}

void cDvdPlayer::SetAudioTrack(eTrackType Type, const tTrackId *TrackId)
{
    if (!TrackId)
        return;
    // cDevice::SetCurrentAudioTrack() calls this holding its
    // mutexCurrentAudioTrack, which the player thread takes in VDR 2.8.3 with
    // the thread lock held, when it announces or clears tracks
    // (SetAvailableTrack(), ClrAvailableTracks()). Waiting for the thread
    // lock here deadlocked the two when the viewer switched tracks with the
    // Audio key: from another thread, the player thread does the switch.
    if (cThread::ThreadId() != playerThreadId) {
        cMutexLock MutexLock(&audioTrackRequestMutex);
        audioTrackRequest = Type;
        audioTrackRequestByViewer = cThread::IsMainThread();
        return;
    }
    SwitchAudioTrack(Type, false);
}

void cDvdPlayer::SwitchRequestedAudioTrack(void)
{
    eTrackType type;
    bool byViewer;
    {
        cMutexLock MutexLock(&audioTrackRequestMutex);
        type = audioTrackRequest;
        byViewer = audioTrackRequestByViewer;
        audioTrackRequest = ttNone;
    }
    if (type != ttNone) {
        LOCK_THREAD;
        SwitchAudioTrack(type, byViewer);
    }
}

void cDvdPlayer::SwitchAudioTrack(eTrackType Type, bool ByViewer)
{
    currentAudioTrack = Type;
    if (!DVDActiveAndRunning())
        return;

    // The stream is in the track's number: VDR selects a track before the
    // first packet registers it, with its id still 0, when libdvdnav
    // switches streams at the start of a title. Taken from the id, that was
    // stream 0, which the player passed on while VDR waited for the other.
    int id = (Type - (IS_DOLBY_TRACK(Type) ? ttDolbyFirst : ttAudioFirst)) & AC3AudioTrackMask;
    int previous = currentNavAudioTrack;

    DEBUG_AUDIO_ID("cDvdPlayer::SetAudioTrack: dd=%d, index=%d, id=0x%2X\n", IS_DOLBY_TRACK(Type), Type, id);

    int i = SearchAudioStream(id);

    if (i < 0 && navAudioTracksSeen.Count() > 0) {
        id = ((IntegerListObject *)navAudioTracksSeen.Get(0))->getValue();
        DEBUG_AUDIO_ID("cDvdPlayer::SetAudioTrack: .. not found !\n");
    } else {
        currentNavAudioTrack = -1;
        DEBUG_AUDIO_ID("cDvdPlayer::SetAudioTrack: .. found !\n");
    }

    SetCurrentNavAudioTrackUsrLocked(true);

    if (currentNavAudioTrack != id) {
        currentNavAudioTrack = id;
        uint16_t audioTrackLangCode = GetAudioTrackLanguageCode(currentNavAudioTrack);

        if (audioTrackLangCode != 0xFFFF) {
            dvdnav_audio_language_select(nav, (char *)&audioTrackLangCode);
        }
#if DVDNAV_VERSION >= DVDNAV_VERSION_CODE(7, 0, 0)
        // And the disc's audio stream register, as a player's Audio button
        // sets it. With the language alone the disc took its first stream of
        // that language again at the next title set - Dolby Digital where
        // the viewer had chosen DTS - and its menu showed the old track.
        int logical = LogicalAudioStream(id);
        if (logical >= 0 && !IsInMenuDomain())
            dvdnav_set_active_stream(nav, (uint8_t)logical, DVD_AUDIO_STREAM);
#endif
        // The viewer's choice, not VDR passing on the player's own (as VDR's
        // player does it): the new track starts with the picture on screen.
        if (previous != id && ByViewer && !IsInMenuDomain()) {
            if (playMode == pmPlay)
                ReplayFromPicture();
            else if (playMode == pmPause)
                replayFromPictureOnPlay = true;
        }
        DEBUG_AUDIO_ID("cDvdPlayer::SetAudioTrack: SWITCHED !\n");
    }
    UpdateTrackInfo();
}

void cDvdPlayer::NoteVobu(const pci_t *Pci)
{
    uint32_t pos, len;
    int32_t title, pgcN, pgN;
    if (dvdnav_get_position(nav, &pos, &len) != DVDNAV_STATUS_OK ||
        dvdnav_current_title_program(nav, &title, &pgcN, &pgN) != DVDNAV_STATUS_OK)
        return;
    cVobuPosition &vobu = vobuHistory[vobuHistoryNext];
    // The player adds ptm_offs to every PTS it passes on.
    vobu.start = Pci->pci_gi.vobu_s_ptm + (uint32_t)ptm_offs;
    vobu.end = Pci->pci_gi.vobu_e_ptm + (uint32_t)ptm_offs;
    vobu.block = pos;
    vobu.title = title;
    vobu.pgcN = pgcN;
    vobuHistoryNext = (vobuHistoryNext + 1) % VobuHistorySize;
    if (vobuHistoryCount < VobuHistorySize)
        vobuHistoryCount++;
}

bool cDvdPlayer::ReplayFromPicture(void)
{
    // Reads on from the VOBU on screen, as VDR's own player does for a new
    // audio track. The device holds a second and more of what was read, so
    // the new track's audio, which starts after that, was that far ahead of
    // the picture: vaapivideo dropped as much video to catch up.
    replayFromPictureOnPlay = false;
    int64_t stc = (int64_t)DeviceGetSTC();
    int32_t title, pgcN, pgN;
    if (stc < 0 || !nav || dvdnav_current_title_program(nav, &title, &pgcN, &pgN) != DVDNAV_STATUS_OK)
        return false;
    for (int i = 1; i <= vobuHistoryCount; i++) {
        const cVobuPosition &vobu = vobuHistory[(vobuHistoryNext - i + VobuHistorySize) % VobuHistorySize];
        // modulo 2^32, as PTS wrap
        if (vobu.title == title && vobu.pgcN == pgcN && uint32_t(stc) - vobu.start < vobu.end - vobu.start) {
            uint32_t block = vobu.block;
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

int cDvdPlayer::DolbyTrackIndex(int SubStreamId)
{
    // VDR registers the AC-3 and DTS tracks of the packets it plays itself,
    // numbered by the substream id's low five bits: DTS streams 0x88-0x8f
    // are its Dolby tracks 8-15. With any other number a track is listed
    // twice, once without its language.
    return SubStreamId & AudioTrackMask;
}

int cDvdPlayer::LogicalAudioStream(int Physical) const
{
    // Despite its name, dvdnav_get_audio_logical_stream() maps a logical
    // audio stream - the number the DVD's attributes and menus use - to the
    // physical one in the MPEG stream, so search for the inverse.
    for (int logical = 0; nav && logical < 8; logical++) {
        if (dvdnav_get_audio_logical_stream(nav, (uint8_t)logical) == Physical)
            return logical;
    }
    return -1;
}

int cDvdPlayer::LogicalSubpStream(int Physical) const
{
    // As for the audio streams, dvdnav_get_spu_logical_stream() maps a logical
    // subpicture stream to its physical one - for a 4:3 picture, or a 16:9 one
    // on a 16:9 TV - so search for the inverse, the current stream first: two
    // logical streams can share a physical one.
    if (!nav || Physical < 0)
        return -1;
    if (currentNavSubpStream >= 0 &&
        dvdnav_get_spu_logical_stream(nav, currentNavSubpStream & SubpStreamMask) == Physical)
        return currentNavSubpStream & SubpStreamMask;
    for (int logical = 0; logical < MaxSubpStreams; logical++) {
        if (dvdnav_get_spu_logical_stream(nav, (uint8_t)logical) == Physical)
            return logical;
    }
    return -1;
}

int cDvdPlayer::PhysicalSubpStream(int Logical) const
{
    // As libdvdnav maps it for a 4:3 picture, or a 16:9 one on a 16:9 TV: the
    // SPU decoder's scale mode stays normal, VDR sets no other.
    if (!nav || Logical < 0)
        return -1;
    int physical = dvdnav_get_spu_logical_stream(nav, Logical & SubpStreamMask);
    return physical >= 0 ? physical & SubpStreamMask : -1;
}

uint16_t cDvdPlayer::GetAudioTrackLanguageCode(int Channel) const
{
    uint16_t audioStreamLanguageCode = 0xFFFF;
    // libdvdnav indexes its table of the 8 logical streams' attributes with
    // whatever number it gets, so only ask for a logical stream that exists.
    int logical = LogicalAudioStream(Channel);
    if (logical >= 0) {
        audioStreamLanguageCode = dvdnav_audio_stream_to_lang(nav, (uint8_t)logical);
        audioStreamLanguageCode = (uint16_t)(audioStreamLanguageCode >> 8 | (audioStreamLanguageCode & 0xFF) << 8);
    }
    return audioStreamLanguageCode;
}

cString cDvdPlayer::AudioTrackDescription(int Channel, const char *LanguageName) const
{
    // VDR's audio menu shows this description in place of the bare language
    // code, so tell the tracks apart by their codec, channel layout and, for
    // commentary or audio-description tracks, a marker - a disc often carries
    // two tracks in the same language that only these distinguish.
    static const char *const Codec[8] = {"AC3", NULL, "MPEG", "MPEG", "LPCM", NULL, "DTS", NULL};
    int logical = LogicalAudioStream(Channel);
    audio_attr_t attr;
    if (logical < 0 || dvdnav_get_audio_attr(nav, (uint8_t)logical, &attr) != DVDNAV_STATUS_OK)
        return cString(LanguageName); // cString(NULL) keeps whatever VDR already has
    cString d(LanguageName ? LanguageName : "");
    if (const char *c = Codec[attr.audio_format & 0x07])
        d = cString::sprintf("%s%s%s", *d, **d ? " " : "", c);
    int channels = attr.channels + 1; // the field holds the count minus one
    if (channels == 6)
        d = cString::sprintf("%s%s5.1", *d, **d ? " " : "");
    else
        d = cString::sprintf("%s%s%d.0", *d, **d ? " " : "", channels);
    const char *ext = attr.code_extension == 2                               ? tr("DVD$visually impaired")
                      : attr.code_extension == 3 || attr.code_extension == 4 ? tr("DVD$comments")
                                                                             : NULL;
    if (ext)
        d = cString::sprintf("%s (%s)", *d, ext);
    return **d ? d : cString(LanguageName);
}

cString cDvdPlayer::SubtitleTrackDescription(int Channel, const char *LanguageName) const
{
    // A DVD often carries several subtitle streams in the same language that
    // differ only in their code extension - forced subtitles for foreign
    // dialogue, captions for the hearing impaired, a director's commentary.
    // Mark those, so VDR's subtitle menu tells them apart.
    subp_attr_t attr;
    if (!nav || Channel < 0 || dvdnav_get_spu_attr(nav, Channel & SubpStreamMask, &attr) != DVDNAV_STATUS_OK)
        return cString(LanguageName);
    const char *ext = attr.code_extension == 9                                 ? tr("DVD$forced")
                      : attr.code_extension >= 5 && attr.code_extension <= 7   ? tr("DVD$captions")
                      : attr.code_extension >= 13 && attr.code_extension <= 15 ? tr("DVD$comments")
                                                                               : NULL;
    if (!ext)
        return cString(LanguageName);
    return LanguageName ? cString::sprintf("%s (%s)", LanguageName, ext) : cString::sprintf("(%s)", ext);
}

uint16_t cDvdPlayer::GetSubtitleLanguageCode(int Channel) const
{
    // Channel is a logical stream: libdvdnav keeps the attributes by those.
    uint16_t subtitleStreamLanguageCode = 0xFFFF;
    if (nav && Channel >= 0) {
        subtitleStreamLanguageCode = dvdnav_spu_stream_to_lang(nav, Channel & SubpStreamMask);
        subtitleStreamLanguageCode =
            (uint16_t)(subtitleStreamLanguageCode >> 8 | (subtitleStreamLanguageCode & 0xFF) << 8);
    }
    return subtitleStreamLanguageCode;
}

void cDvdPlayer::ReplaceInfoString(char *&Str, char *New)
{
    char *old;
    {
        cMutexLock MutexLock(&infoMutex);
        old = Str;
        Str = New;
    }
    free(old);
}

char *cDvdPlayer::GetTitleString() const
{
    cMutexLock MutexLock(&infoMutex);
    return strdup(title_str);
}

void cDvdPlayer::SetTitleString()
{
    const char *title_holder = NULL;

    if (DVDActiveAndRunning() && dvdnav_get_title_string(nav, &title_holder) == DVDNAV_STATUS_OK)
        ReplaceInfoString(title_str, strdup(title_holder));
    else
        ReplaceInfoString(title_str, strdup(dummy_title));
}

char *cDvdPlayer::GetTitleInfoString() const
{
    cMutexLock MutexLock(&infoMutex);
    return strdup(titleinfo_str);
}

void cDvdPlayer::SetTitleInfoString()
{
    if (!DVDActiveAndRunning()) {
        ReplaceInfoString(titleinfo_str, strdup(dummy_title));
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
    if (titleNumber >= 1)
        dvdnav_get_angle_info(nav, &angleNumber, &angleNumbers);

    cString info;
    if (titleNumber == 0)
        info = cString::sprintf("%d/%d %d", titleNumber, titleNumbers, chapterNumber);
    else if (angleNumbers > 1)
        info = cString::sprintf("%d/%d %d/%d %d/%d", titleNumber, titleNumbers, chapterNumber, chapterNumbers,
                                angleNumber, angleNumbers);
    else
        info = cString::sprintf("%d/%d %d/%d", titleNumber, titleNumbers, chapterNumber, chapterNumbers);
    ReplaceInfoString(titleinfo_str, strdup(info));
}

void cDvdPlayer::UpdateTrackInfo(void)
{
    char audio[100];
    char subtitle[100];

    if (!DVDActiveAndRunning()) {
        strcpy(audio, "n.a.");
        strcpy(subtitle, "n.a.");
    } else {
        const char *audioTypeDescr;
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

        uint16_t audioStreamLanguageCode = GetAudioTrackLanguageCode(currentNavAudioTrack);
        char audioLanguageStr[3] = {(char)audioStreamLanguageCode, (char)(audioStreamLanguageCode >> 8), 0};
        if (GetAudioStreamNumbers() > 1)
            snprintf(audio, sizeof(audio), "%s %d/%d %s", audioStreamLanguageCode != 0xFFFF ? audioLanguageStr : "",
                     GetCurrentNavAudioTrackIdx() + 1, GetAudioStreamNumbers(), audioTypeDescr);
        else
            snprintf(audio, sizeof(audio), "%s %s", audioStreamLanguageCode != 0xFFFF ? audioLanguageStr : "",
                     audioTypeDescr);

        // As in VDR's menu, forced subtitles only count as none.
        int shownSubpStream = SubtitlesShown(currentNavSubpStream) ? currentNavSubpStream : -1;
        int subtitleStreamLanguageCode = GetSubtitleLanguageCode(shownSubpStream);
        char subtitleLanguageStr[3] = {(char)subtitleStreamLanguageCode, (char)(subtitleStreamLanguageCode >> 8), 0};
        if (GetSubtitleStreamNumbers() > 2)
            snprintf(subtitle, sizeof(subtitle), "%s %d/%d",
                     subtitleStreamLanguageCode != 0xFFFF ? subtitleLanguageStr : "", SearchSubpStream(shownSubpStream),
                     GetSubtitleStreamNumbers() - 1);
        else if (GetSubtitleStreamNumbers() > 1)
            snprintf(subtitle, sizeof(subtitle), "%s",
                     subtitleStreamLanguageCode != 0xFFFF ? subtitleLanguageStr : "n.a.");
        else
            strcpy(subtitle, "n.a."); // no subtitles, or only the "no subtitles" entry
    }

    cMutexLock MutexLock(&infoMutex);
    strcpy(infoAudio, audio);
    strcpy(infoSubtitle, subtitle);
}

void cDvdPlayer::GetAudioLanguageStr(const char **AudioLanguageStr) const
{
    static char buffer[100]; // only the control's thread calls this
    cMutexLock MutexLock(&infoMutex);
    strn0cpy(buffer, infoAudio, sizeof(buffer));
    *AudioLanguageStr = buffer;
}

void cDvdPlayer::GetSubtitleLanguageStr(const char **SubtitleLanguageStr) const
{
    static char buffer[100]; // only the control's thread calls this
    cMutexLock MutexLock(&infoMutex);
    strn0cpy(buffer, infoSubtitle, sizeof(buffer));
    *SubtitleLanguageStr = buffer;
}

char *cDvdPlayer::GetAspectString() const
{
    cMutexLock MutexLock(&infoMutex);
    return strdup(aspect_str);
}

void cDvdPlayer::SetAspectString()
{
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
    ReplaceInfoString(aspect_str, strdup(aspect));
}

bool cDvdPlayer::GetReplayMode(bool &Play, bool &Forward, int &Speed)
{
    Play = (playMode == pmPlay || playMode == pmFast);
    Forward = (playDir == pdForward);
    if (playMode == pmFast || playMode == pmSlow)
        Speed = Setup.MultiSpeedMode ? abs(trickSpeed - NORMAL_SPEED) : 0;
    else
        Speed = -1;
    return true;
}

int cDvdPlayer::callRootMenu(void)
{
    DEBUGDVD("cDvdPlayer::callRootMenu()\n");

    LOCK_THREAD;
    SetCurrentNavAudioTrackUsrLocked(false);
    return MenuCall(DVD_MENU_Root);
}

int cDvdPlayer::callTitleMenu(void)
{
    DEBUGDVD("cDvdPlayer::callTitleMenu()\n");

    LOCK_THREAD;
    SetCurrentNavAudioTrackUsrLocked(false);
    return MenuCall(DVD_MENU_Title);
}

int cDvdPlayer::callSubpMenu(void)
{
    DEBUGDVD("cDvdPlayer::callSubpMenu()\n");

    LOCK_THREAD;
    return MenuCall(DVD_MENU_Subpicture);
}

int cDvdPlayer::callAudioMenu(void)
{
    DEBUGDVD("cDvdPlayer::callAudioMenu()\n");

    LOCK_THREAD;
    SetCurrentNavAudioTrackUsrLocked(false);
    return MenuCall(DVD_MENU_Audio);
}
