/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#include "control-dvd.h"
#include "common-dvd.h"
#include "player-dvd.h"

// C++ Standard Library
#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <utility>

// VDR
#include <vdr/config.h>
#include <vdr/i18n.h>
#include <vdr/keys.h>
#include <vdr/osd.h>
#include <vdr/osdbase.h>
#include <vdr/player.h>
#include <vdr/recording.h>
#include <vdr/skins.h>
#include <vdr/status.h>
#include <vdr/thread.h>
#include <vdr/tools.h>

constexpr int MODETIMEOUT = 3; // seconds

bool cDvdPlayerControl::dvd_active = false;

// --- cDvdPlayerControl -----------------------------------------------------

// Its address marks the OSD as released by the control: OsdTaken() counts it
// as taken, by an OSD VDR may have opened, until VDR has none open.
static char osdReleasedTag;
static void *const OsdReleased = &osdReleasedTag;

cDvdPlayerControl::cDvdPlayerControl() : cDvdPlayerControl(new cDvdPlayer()) {}

cDvdPlayerControl::cDvdPlayerControl(cDvdPlayer *Player) : cControl(Player), player(Player) {
    assert(dvd_active == false);
    dvd_active = true;
    player->setController(this);
    cStatus::MsgReplaying(this, "DVD", nullptr, true);
}

cDvdPlayerControl::~cDvdPlayerControl() {
    cDvdPlayerControl::Hide();
    cStatus::MsgReplaying(this, nullptr, nullptr, false);
    Stop();
    assert(dvd_active == true);
    dvd_active = false;
    delete player;
}

auto cDvdPlayerControl::Active() -> bool { return player->Active(); }

void cDvdPlayerControl::Stop() { player->Stop(); }

void cDvdPlayerControl::Pause() { player->Pause(); }

void cDvdPlayerControl::Play() { player->Play(); }

void cDvdPlayerControl::Forward() { player->Forward(); }

void cDvdPlayerControl::Backward() { player->Backward(); }

void cDvdPlayerControl::SkipSeconds(int Seconds) { player->SkipSeconds(Seconds); }

void cDvdPlayerControl::Goto(int Seconds) { player->Goto(Seconds); }

void cDvdPlayerControl::OsdClose() { DELETENULL(displayReplay); }

void cDvdPlayerControl::OsdOpen() {
    if (visible) {
        DEBUG_OSDCTRL("DVD-Ctrl: OsdOpen: already visible\n");
        return;
    }

    // Under the player's lock, which its thread holds from checking that no
    // other OSD is open to drawing the subpicture: the display opened in
    // between, and the subpicture or the display got a dummy OSD from VDR.
    const cThreadLock PlayerLock(player->Thread());
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

void cDvdPlayerControl::ShowTimed(int Seconds) {
    if (modeOnly) {
        Hide();
    }
    if (!visible) {
        shown = ShowProgress(true);
        timeoutShow = (shown && Seconds > 0) ? time(nullptr) + Seconds : 0;
    }
}

void cDvdPlayerControl::Show() { ShowTimed(); }

void cDvdPlayerControl::Hide() {
    DEBUG_SHOW("DVD-Ctrl: Hide\n");

    TakeOsd(OsdReleased); // another OSD may open now
    player->HideSPU();
    HideOwnOsd();
}

void cDvdPlayerControl::HideOwnOsd() {
    DEBUG_SHOW("DVD-Ctrl: HideOwnOsd: visible=%d\n", visible.load());

    if (visible) {
        OsdClose();
        SetNeedsFastResponse(false);
        visible = modeOnly = false;
        lastPlay = lastForward = false;
        lastSpeed = -1;
    }
}

auto cDvdPlayerControl::TakeOsd(void *owner) -> bool {
    if (owner == OsdReleased || !OsdTaken(owner) || osdTaker == OsdReleased) {
        DEBUG_OSDCTRL("DVD-Ctrl: TakeOsd(%p) new owner!\n", owner);
        osdTaker = owner;
        return true;
    }
    DEBUG_OSDCTRL("DVD-Ctrl: TakeOsd(%p) not taken! %p \n", owner, osdTaker.load());

    return false;
}

auto cDvdPlayerControl::OsdTaken(const void *me) -> bool {
    if (osdTaker && !cOsd::IsOpen()) {
        DEBUG_OSDCTRL("DVD-Ctrl: OsdTaken(%p) !IsOpen->taker:=NULL\n", me);
        osdTaker = nullptr; // the owner's OSD has been closed
    }
    DEBUG_OSDCTRL("DVD-Ctrl: OsdTaken(%p) != %p := %d \n", me, osdTaker.load(), osdTaker != me);
    return osdTaker != nullptr && osdTaker != me;
}

auto cDvdPlayerControl::OsdVisible(const void *me) -> bool { return visible || OsdTaken(me); }

void cDvdPlayerControl::ShowMode() {
    if (Setup.ShowReplayMode && inputActive == NoneInput) {
        bool Playing = false;
        bool Forwards = false;
        int Speed = 0;
        if (GetReplayMode(Playing, Forwards, Speed) &&
            (!visible || Playing != lastPlay || Forwards != lastForward || Speed != lastSpeed)) {
            const bool NormalPlay = (Playing && Speed == -1);
            if (!visible) {
                if (NormalPlay) {
                    return; // no need to indicate ">" unless a different mode was shown before
                }
                modeOnly = true;
                OsdOpen(); // the small display
            }
            if (!visible) {
                return;
            }
            if (modeOnly && !timeoutShow && NormalPlay) {
                timeoutShow = time(nullptr) + MODETIMEOUT;
            }
            displayReplay->SetMode(Playing, Forwards, Speed);
            lastPlay = Playing;
            lastForward = Forwards;
            lastSpeed = Speed;
        }
    }
}

auto cDvdPlayerControl::ShowProgress(bool Initial) -> bool {
    int Current = 0;
    int Total = 0;

    if (GetIndex(Current, Total) && Total > 0) {
        DEBUG_SHOW("DVD-Ctrl: ShowProgress: ... \n");
        if (!visible) {
            SetNeedsFastResponse(true);
            OsdOpen();
        }
        if (!visible) {
            return false;
        }

        if (Initial) {
            lastCurrent = lastTotal = -1;
            shownTitle = nullptr;
            displayReplay->SetTitle("unknown title");
            cStatus::MsgReplaying(this, "unknown title", nullptr, true);
        }

        cString title = player->GetDisplayHeaderLine();
        if (!*shownTitle || strcmp(title, shownTitle) != 0) {
            displayReplay->SetTitle(title);
            if (!Initial) {
                displayReplay->Flush();
            }
            cStatus::MsgReplaying(this, title, nullptr, true);
            shownTitle = std::move(title);
        }

        if (Current != lastCurrent || Total != lastTotal) {
            displayReplay->SetProgress(Current, Total);
            displayReplay->SetTotal(IndexToHMSF(Total));
            displayReplay->SetCurrent(IndexToHMSF(Current, displayFrames));
            lastCurrent = Current;
            if (!Initial) {
                displayReplay->Flush();
            }
        }
        lastTotal = Total;
        ShowMode();
        return true;
    }
    DEBUG_SHOW("DVD-Ctrl: ShowProgress: nope \n");

    return false;
}

void cDvdPlayerControl::InputIntDisplay(const char *msg, int val) {
    char buf[120];
    snprintf(buf, sizeof(buf), "%s %d", msg, val);
    displayReplay->SetJump(buf);
}

void cDvdPlayerControl::InputIntProcess(eKeys Key, const char *msg, int &val) {
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
            val = (val * 10) + Key - k0;
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
        if (inputHide) {
            Hide();
        } else {
            displayReplay->SetJump(nullptr);
        }
        ShowMode();
    }
}

void cDvdPlayerControl::TrackSearch() {
    inputIntVal = 0;
    inputIntMsg = "Track: ";
    inputHide = false;
    Show();
    if (visible) {
        inputHide = true;
    } else {
        return;
    }
    timeoutShow = 0;
    InputIntDisplay(inputIntMsg, inputIntVal);
    inputActive = TrackSearchInput;
}

void cDvdPlayerControl::TimeSearchDisplay() {
    const char h10 = static_cast<char>('0' + (timeSearchTime >> 24));
    const char h1 = static_cast<char>('0' + ((timeSearchTime & 0x00FF0000) >> 16));
    const char m10 = static_cast<char>('0' + ((timeSearchTime & 0x0000FF00) >> 8));
    const char m1 = static_cast<char>('0' + (timeSearchTime & 0x000000FF));
    const char ch10 = timeSearchPos > 3 ? h10 : '-';
    const char ch1 = timeSearchPos > 2 ? h1 : '-';
    const char cm10 = timeSearchPos > 1 ? m10 : '-';
    const char cm1 = timeSearchPos > 0 ? m1 : '-';
    displayReplay->SetJump(cString::sprintf("%s%c%c:%c%c", trVDR("Jump: "), ch10, ch1, cm10, cm1));
}

void cDvdPlayerControl::TimeSearchProcess(eKeys Key) {
    constexpr int STAY_SECONDS_OFF_END = 10;
    int Seconds = ((timeSearchTime >> 24) * 36000) + (((timeSearchTime & 0x00FF0000) >> 16) * 3600) +
                  (((timeSearchTime & 0x0000FF00) >> 8) * 600) + ((timeSearchTime & 0x000000FF) * 60);
    const int Current = static_cast<int>(lastCurrent / FramesPerSecond());
    const int Total = static_cast<int>(lastTotal / FramesPerSecond());
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
            const int dir = (Key == kRight ? 1 : -1);
            if (dir > 0) {
                Seconds = min(Total - Current - STAY_SECONDS_OFF_END, Seconds);
            }
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
        if (inputHide) {
            Hide();
        } else {
            displayReplay->SetJump(nullptr);
        }
        ShowMode();
    }
}

void cDvdPlayerControl::TimeSearch() {
    timeSearchTime = timeSearchPos = 0;
    inputHide = false;
    Show();
    if (visible) {
        inputHide = true;
    } else {
        return;
    }
    timeoutShow = 0;
    TimeSearchDisplay();
    inputActive = TimeSearchInput;
}

auto cDvdPlayerControl::DvdNavigation(eKeys Key) -> bool {
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

void cDvdPlayerControl::UpdateShow(bool force) {
    DEBUG_SHOW("DVD-Ctrl: updateShow: force=%d, visible=%d, modeOnly=%d\n", force, visible.load(), modeOnly);
    if (visible || force) {
        if (timeoutShow && time(nullptr) > timeoutShow) {
            Hide();
            ShowMode();
            timeoutShow = 0;
            return;
        }
        if (modeOnly) {
            ShowMode();
        } else {
            shown = ShowProgress(!shown) || shown;
        }
    } else {
        const cString title = player->GetDisplayHeaderLine();
        if (!*reportedTitle || strcmp(title, reportedTitle) != 0) {
            reportedTitle = title;
            cStatus::MsgReplaying(this, title, nullptr, true);
        }
    }
}

auto cDvdPlayerControl::ProcessKey(eKeys Key) -> eOSState {
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

    const bool DisplayedFrames = displayFrames;
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

        switch (static_cast<int>(Key)) {
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
                if (DvdNavigation(Key)) {
                    break;
                }
                // fall through
            default:
                state = osUnknown;
                break;
        }
    } else {
        switch (static_cast<int>(Key)) {
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
                if (Setup.MultiSpeedMode) {
                    break;
                }
                // fall through
            case kFastRew:
            case kLeft:
                Backward();
                break;
            case kFastFwd | k_Release:
            case kRight | k_Release:
                if (Setup.MultiSpeedMode) {
                    break;
                }
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

        switch (static_cast<int>(Key)) {
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
                switch (static_cast<int>(Key)) {
                    // Menu control:
                    case kOk:
                        if (visible && !modeOnly) {
                            Hide();
                            DoShowMode = true;
                        } else {
                            Show();
                        }
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
