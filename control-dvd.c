/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#include <assert.h>
#include <utility>
#include <vdr/i18n.h>
#include <vdr/thread.h>
#include <vdr/skins.h>

#include "tools-dvd.h"
#include "player-dvd.h"
#include "control-dvd.h"

#define MODETIMEOUT 3 // seconds

bool cDvdPlayerControl::dvd_active = false;

// --- cDvdPlayerControl -----------------------------------------------------

// Its address marks the OSD as released by the control: OsdTaken() counts it
// as taken, by an OSD VDR may have opened, until VDR has none open.
static char osdReleasedTag;
static void *const OsdReleased = &osdReleasedTag;

cDvdPlayerControl::cDvdPlayerControl(void)
    : cControl(new cDvdPlayer()), player(static_cast<cDvdPlayer *>(cControl::player))
{
    assert(dvd_active == false);
    dvd_active = true;
    visible = modeOnly = shown = displayFrames = false;
    osdTaker = NULL;
    lastCurrent = lastTotal = -1;
    lastPlay = lastForward = false;
    lastSpeed = -1;
    displayReplay = NULL;
    timeoutShow = 0;
    inputActive = NoneInput;
    inputHide = true;
    forceDvdNavigation = false;
    player->setController(this);
    cStatus::MsgReplaying(this, "DVD", NULL, true);
}

cDvdPlayerControl::~cDvdPlayerControl()
{
    cDvdPlayerControl::Hide();
    cStatus::MsgReplaying(this, NULL, NULL, false);
    Stop();
    assert(dvd_active == true);
    dvd_active = false;
    delete player;
}

bool cDvdPlayerControl::Active(void)
{
    return player->Active();
}

void cDvdPlayerControl::Stop(void)
{
    player->Stop();
}

void cDvdPlayerControl::Pause(void)
{
    player->Pause();
}

void cDvdPlayerControl::Play(void)
{
    player->Play();
}

void cDvdPlayerControl::Forward(void)
{
    player->Forward();
}

void cDvdPlayerControl::Backward(void)
{
    player->Backward();
}

void cDvdPlayerControl::SkipSeconds(int Seconds)
{
    player->SkipSeconds(Seconds);
}

void cDvdPlayerControl::Goto(int Seconds)
{
    player->Goto(Seconds);
}

void cDvdPlayerControl::OsdClose()
{
    DELETENULL(displayReplay);
}

void cDvdPlayerControl::OsdOpen(void)
{
    if (visible) {
        DEBUG_OSDCTRL("DVD-Ctrl: OsdOpen: already visible\n");
        return;
    }

    // Under the player's lock, which its thread holds from checking that no
    // other OSD is open to drawing the subpicture: the display opened in
    // between, and the subpicture or the display got a dummy OSD from VDR.
    cThreadLock PlayerLock(player->Thread());
    if (OsdTaken(this)) {
        DEBUG_OSDCTRL("DVD-Ctrl: OsdOpen: already open -> HideSPU\n");
        player->HideSPU();
    }
    if (!TakeOsd(this)) {
        DEBUG_OSDCTRL("DVD-Ctrl: OsdOpen: already open -> !visible, osdTaken\n");
        visible = false;
    } else {
        DEBUG_OSDCTRL("DVD-Ctrl: OsdOpen: visible\n");
        visible = true;
        displayReplay = Skins.Current()->DisplayReplay(modeOnly);
    }
}

void cDvdPlayerControl::ShowTimed(int Seconds)
{
    if (modeOnly)
        Hide();
    if (!visible) {
        shown = ShowProgress(true);
        timeoutShow = (shown && Seconds > 0) ? time(NULL) + Seconds : 0;
    }
}

void cDvdPlayerControl::Show(void)
{
    ShowTimed();
}

void cDvdPlayerControl::Hide(void)
{
    DEBUG_SHOW("DVD-Ctrl: Hide\n");

    TakeOsd(OsdReleased); // another OSD may open now
    player->HideSPU();
    HideOwnOsd();
}

void cDvdPlayerControl::HideOwnOsd(void)
{
    DEBUG_SHOW("DVD-Ctrl: HideOwnOsd: visible=%d\n", visible.load());

    if (visible) {
        OsdClose();
        SetNeedsFastResponse(false);
        visible = modeOnly = false;
        lastPlay = lastForward = false;
        lastSpeed = -1;
    }
}

bool cDvdPlayerControl::TakeOsd(void *owner)
{
    if (owner == OsdReleased || !OsdTaken(owner) || osdTaker == OsdReleased) {
        DEBUG_OSDCTRL("DVD-Ctrl: TakeOsd(%p) new owner!\n", owner);
        osdTaker = owner;
        return true;
    } else {
        DEBUG_OSDCTRL("DVD-Ctrl: TakeOsd(%p) not taken! %p \n", owner, osdTaker.load());
    }
    return false;
}

bool cDvdPlayerControl::OsdTaken(const void *me)
{
    if (osdTaker && !cOsd::IsOpen()) {
        DEBUG_OSDCTRL("DVD-Ctrl: OsdTaken(%p) !IsOpen->taker:=NULL\n", me);
        osdTaker = NULL; // the owner's OSD has been closed
    }
    DEBUG_OSDCTRL("DVD-Ctrl: OsdTaken(%p) != %p := %d \n", me, osdTaker.load(), osdTaker != me);
    return osdTaker != NULL && osdTaker != me;
}

bool cDvdPlayerControl::OsdVisible(const void *me)
{
    return visible || OsdTaken(me);
}

void cDvdPlayerControl::ShowMode(void)
{
    if (Setup.ShowReplayMode && inputActive == NoneInput) {
        bool Playing, Forwards;
        int Speed;
        if (GetReplayMode(Playing, Forwards, Speed) &&
            (!visible || Playing != lastPlay || Forwards != lastForward || Speed != lastSpeed)) {
            bool NormalPlay = (Playing && Speed == -1);
            if (!visible) {
                if (NormalPlay)
                    return; // no need to indicate ">" unless a different mode was shown before
                modeOnly = true;
                OsdOpen(); // the small display
            }
            if (!visible)
                return;
            if (modeOnly && !timeoutShow && NormalPlay)
                timeoutShow = time(NULL) + MODETIMEOUT;
            displayReplay->SetMode(Playing, Forwards, Speed);
            lastPlay = Playing;
            lastForward = Forwards;
            lastSpeed = Speed;
        }
    }
}

cString cDvdPlayerControl::GetDisplayHeaderLine(void)
{
    const char *audiolang_str = NULL;
    const char *spulang_str = NULL;
    char *titleinfo_str = player->GetTitleInfoString();
    char *title_str = player->GetTitleString();
    char *aspect_str = player->GetAspectString();
    player->GetAudioLanguageStr(&audiolang_str);
    player->GetSubtitleLanguageStr(&spulang_str);

    cString line =
        cString::sprintf("%s, %s, %s, %s, %s    ", titleinfo_str, audiolang_str, spulang_str, aspect_str, title_str);
    free(titleinfo_str);
    free(title_str);
    free(aspect_str);
    return line;
}

bool cDvdPlayerControl::ShowProgress(bool Initial)
{
    int Current, Total;

    if (GetIndex(Current, Total) && Total > 0) {
        DEBUG_SHOW("DVD-Ctrl: ShowProgress: ... \n");
        if (!visible) {
            SetNeedsFastResponse(true);
            OsdOpen();
        }
        if (!visible)
            return false;

        if (Initial) {
            lastCurrent = lastTotal = -1;
            shownTitle = NULL;
            displayReplay->SetTitle("unknown title");
            cStatus::MsgReplaying(this, "unknown title", NULL, true);
        }

        cString title = GetDisplayHeaderLine();
        if (!*shownTitle || strcmp(title, shownTitle) != 0) {
            displayReplay->SetTitle(title);
            if (!Initial)
                displayReplay->Flush();
            cStatus::MsgReplaying(this, title, NULL, true);
            shownTitle = std::move(title);
        }

        if (Current != lastCurrent || Total != lastTotal) {
            displayReplay->SetProgress(Current, Total);
            displayReplay->SetTotal(IndexToHMSF(Total));
            displayReplay->SetCurrent(IndexToHMSF(Current, displayFrames));
            lastCurrent = Current;
            if (!Initial)
                displayReplay->Flush();
        }
        lastTotal = Total;
        ShowMode();
        return true;
    } else {
        DEBUG_SHOW("DVD-Ctrl: ShowProgress: nope \n");
    }
    return false;
}

void cDvdPlayerControl::InputIntDisplay(const char *msg, int val)
{
    char buf[120];
    snprintf(buf, sizeof(buf), "%s %d", msg, val);
    displayReplay->SetJump(buf);
}

void cDvdPlayerControl::InputIntProcess(eKeys Key, const char *msg, int &val)
{
    switch (Key) {
        case k0:
        case k1:
        case k2:
        case k3:
        case k4:
        case k5:
        case k6:
        case k7:
        case k8:
        case k9:
            val = val * 10 + Key - k0;
            InputIntDisplay(msg, val);
            break;
        case kOk:
        case kLeft:
        case kRight:
        case kUp:
        case kDown:
            player->GotoTitle(val);
            inputActive = NoneInput;
            break;
        default:
            inputActive = NoneInput;
            break;
    }
    if (inputActive == NoneInput) {
        if (inputHide)
            Hide();
        else
            displayReplay->SetJump(NULL);
        ShowMode();
    }
}

void cDvdPlayerControl::TrackSearch(void)
{
    inputIntVal = 0;
    inputIntMsg = "Track: ";
    inputHide = false;
    Show();
    if (visible)
        inputHide = true;
    else
        return;
    timeoutShow = 0;
    InputIntDisplay(inputIntMsg, inputIntVal);
    inputActive = TrackSearchInput;
}

void cDvdPlayerControl::TimeSearchDisplay(void)
{
    char h10 = char('0' + (timeSearchTime >> 24));
    char h1 = char('0' + ((timeSearchTime & 0x00FF0000) >> 16));
    char m10 = char('0' + ((timeSearchTime & 0x0000FF00) >> 8));
    char m1 = char('0' + (timeSearchTime & 0x000000FF));
    char ch10 = timeSearchPos > 3 ? h10 : '-';
    char ch1 = timeSearchPos > 2 ? h1 : '-';
    char cm10 = timeSearchPos > 1 ? m10 : '-';
    char cm1 = timeSearchPos > 0 ? m1 : '-';
    displayReplay->SetJump(cString::sprintf("%s%c%c:%c%c", trVDR("Jump: "), ch10, ch1, cm10, cm1));
}

void cDvdPlayerControl::TimeSearchProcess(eKeys Key)
{
#define STAY_SECONDS_OFF_END 10
    int Seconds = (timeSearchTime >> 24) * 36000 + ((timeSearchTime & 0x00FF0000) >> 16) * 3600 +
                  ((timeSearchTime & 0x0000FF00) >> 8) * 600 + (timeSearchTime & 0x000000FF) * 60;
    int Current = int(lastCurrent / FramesPerSecond());
    int Total = int(lastTotal / FramesPerSecond());
    switch (Key) {
        case k0:
        case k1:
        case k2:
        case k3:
        case k4:
        case k5:
        case k6:
        case k7:
        case k8:
        case k9:
            if (timeSearchPos < 4) {
                timeSearchTime <<= 8;
                timeSearchTime |= Key - k0;
                timeSearchPos++;
                TimeSearchDisplay();
            }
            break;
        case kLeft:
        case kRight: {
            int dir = (Key == kRight ? 1 : -1);
            if (dir > 0)
                Seconds = min(Total - Current - STAY_SECONDS_OFF_END, Seconds);
            SkipSeconds(Seconds * dir);
            inputActive = NoneInput;
            break;
        }
        case kPlay:
        case kUp:
        case kPause:
        case kDown:
        case kOk:
            Seconds = min(Total - STAY_SECONDS_OFF_END, Seconds);
            Goto(Seconds);
            inputActive = NoneInput;
            break;
        default:
            inputActive = NoneInput;
            break;
    }
    if (inputActive == NoneInput) {
        if (inputHide)
            Hide();
        else
            displayReplay->SetJump(NULL);
        ShowMode();
    }
}

void cDvdPlayerControl::TimeSearch(void)
{
    timeSearchTime = timeSearchPos = 0;
    inputHide = false;
    Show();
    if (visible)
        inputHide = true;
    else
        return;
    timeoutShow = 0;
    TimeSearchDisplay();
    inputActive = TimeSearchInput;
}

bool cDvdPlayerControl::DvdNavigation(eKeys Key)
{
    HideOwnOsd();
    player->DrawSPU();

    switch (Key) {
        case kUp:
        case k2:
            player->selectUpButton();
            break;
        case kDown:
        case k8:
            player->selectDownButton();
            break;
        case kLeft:
        case k4:
            player->selectLeftButton();
            break;
        case kRight:
        case k6:
            player->selectRightButton();
            break;
        case kOk:
        case k5:
            player->activateButton();
            break;
        case k1:
            player->callRootMenu();
            break;
        case k3:
            player->callTitleMenu();
            break;
        case k7:
            player->callSubpMenu();
            break;
        case k9:
            player->callAudioMenu();
            break;
        default:
            return false;
    }
    return true;
}

void cDvdPlayerControl::UpdateShow(bool force)
{
    DEBUG_SHOW("DVD-Ctrl: updateShow: force=%d, visible=%d, modeOnly=%d\n", force, visible.load(), modeOnly);
    if (visible || force) {
        if (timeoutShow && time(NULL) > timeoutShow) {
            Hide();
            ShowMode();
            timeoutShow = 0;
            return;
        }
        if (modeOnly)
            ShowMode();
        else
            shown = ShowProgress(!shown) || shown;
    } else {
        cString title = GetDisplayHeaderLine();
        if (!*reportedTitle || strcmp(title, reportedTitle) != 0) {
            reportedTitle = title;
            cStatus::MsgReplaying(this, title, NULL, true);
        }
    }
}

eOSState cDvdPlayerControl::ProcessKey(eKeys Key)
{
    DEBUG_KEY("DVD-Ctrl: ProcessKey BEGIN\n");

    eOSState state = cOsdObject::ProcessKey(Key);

    if (state != osUnknown) {
        Hide();
        DEBUG_KEY("cDvdPlayerControl::ProcessKey key: %d 0x%X, state=%d 0x%X FIN!\n", Key, Key, state, state);
        DEBUG_KEY("DVD-Ctrl: ProcessKey END\n");
        return state;
    }

    DEBUG_KEY("cDvdPlayerControl::ProcessKey key: %d 0x%X, state=unknown\n", Key, Key);

    if (player->DVDRemoveable()) {
        DEBUG_KEY("cDvdPlayerControl::ProcessKey DVDRemoveable -> Stop\n");
        Hide();
        DEBUG_KEY("DVD-Ctrl: ProcessKey END\n");
        return osEnd;
    }
    if (!Active()) {
        DEBUG_KEY("cDvdPlayerControl::ProcessKey !Active -> Stop\n");
        Hide();
        Stop();
        DEBUG_KEY("DVD-Ctrl: ProcessKey END\n");
        return osEnd;
    }

    UpdateShow();

    bool DisplayedFrames = displayFrames;
    displayFrames = false;
    if (inputActive != NoneInput && Key != kNone) {
        switch (inputActive) {
            case TimeSearchInput:
                TimeSearchProcess(Key);
                break;
            case TrackSearchInput:
                InputIntProcess(Key, inputIntMsg, inputIntVal);
                break;
            default:
                break;
        }
        DEBUG_KEY("DVD-Ctrl: ProcessKey END\n");
        return osContinue;
    }
    bool DoShowMode = true;

    state = osContinue;

    if (player->IsInMenuDomain() || forceDvdNavigation) {

        switch (int(Key)) {
            case kRed:
                forceDvdNavigation = false;
                break;
            case kGreen | k_Repeat:
            case kGreen:
                player->PreviousTitle();
                break;
            case kYellow | k_Repeat:
            case kYellow:
                player->NextTitle();
                break;
            case kUp:
            case kDown:
            case kLeft:
            case kRight:
            case kOk:
            case k0:
            case k1:
            case k2:
            case k3:
            case k4:
            case k5:
            case k6:
            case k7:
            case k8:
            case k9:
                DoShowMode = false;
                displayFrames = DisplayedFrames;
                if (DvdNavigation(Key))
                    break;
                // fall through
            default:
                state = osUnknown;
                break;
        }
    } else {
        switch (int(Key)) {
            // Positioning:
            case kPlay:
            case kUp:
                Play();
                break;
            case kPause:
            case kDown:
                Pause();
                break;
            case kFastRew | k_Release:
            case kLeft | k_Release:
                if (Setup.MultiSpeedMode)
                    break;
                // fall through
            case kFastRew:
            case kLeft:
                Backward();
                break;
            case kFastFwd | k_Release:
            case kRight | k_Release:
                if (Setup.MultiSpeedMode)
                    break;
                // fall through
            case kFastFwd:
            case kRight:
                Forward();
                break;
            default:
                state = osUnknown;
                break;
        }
    }

    if (state == osUnknown) {
        state = osContinue;

        switch (int(Key)) {
            // Positioning:
            case kRed:
                TimeSearch();
                break;
            case kGreen | k_Repeat:
            case kGreen:
                SkipSeconds(-60);
                break;
            case kYellow | k_Repeat:
            case kYellow:
                SkipSeconds(60);
                break;
            case kBlue:
                TrackSearch();
                break;
            default: {
                DoShowMode = false;
                displayFrames = DisplayedFrames;
                switch (int(Key)) {
                    // Menu control:
                    case kOk:
                        if (visible && !modeOnly) {
                            Hide();
                            DoShowMode = true;
                        } else
                            Show();
                        break;
                    case kStop:
                    case kBack:
                        Hide();
                        Stop();
                        state = osEnd;
                        break;
                    case k2:
                        player->NextSubpStream();
                        if (player->GetCurrentNavSubpStream() == -1) {
                            Hide();
                            Show();
                        }
                        break;
                    case k3:
                        player->NextAngle();
                        break;
                    case kPrev:
                    case k4:
                        player->PreviousPart();
                        break;
                    case kNext:
                    case k6:
                        player->NextPart();
                        break;
                    case kChanDn:
                    case k7:
                        player->PreviousTitle();
                        break;
                    case kChanUp:
                    case k9:
                        player->NextTitle();
                        break;
                    case k5:
                        if (visible && !modeOnly) {
                            Hide();
                            player->callRootMenu();
                        } else {
                            Hide();
                            forceDvdNavigation = true;
                        }
                        break;
                    case k8:
                        Hide();
                        player->callTitleMenu();
                        break;
                    case k0:
                        Hide();
                        player->callAudioMenu();
                        break;
                    default:
                        state = osUnknown;
                }
            }
        }
    }
    if (DoShowMode && state != osEnd) {
        DEBUG_SHOW("cDvdPlayerControl::ProcessKey ShowMode\n");
        ShowMode();
    }
    DEBUG_KEY("DVD-Ctrl: ProcessKey END\n");
    return state;
}
