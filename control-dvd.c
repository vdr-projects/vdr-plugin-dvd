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
#include <vdr/i18n.h>
#include <vdr/thread.h>
//#include <vdr/menu.h>

#include "tools-dvd.h"
#include "player-dvd.h"
#include "control-dvd.h"

#define MENUTIMEOUT     120 // seconds
#define MAXWAIT4EPGINFO  10 // seconds
#define MODETIMEOUT       3 // seconds

bool cDvdPlayerControl::dvd_active = false;

#if VDRVERSNUM<10307

#ifdef OSDPOSDEBUG
    #define DEBUG_OSDPOS(format, args...) printf (format, ## args)
#else
    #define DEBUG_OSDPOS(format, args...)
#endif

// --- cDvdProgressBar --------------------------------------------------------

class cDvdProgressBar : public cBitmap {
protected:
    int total;
    int Pos(int p) { return p * width / total; }
public:
    cDvdProgressBar(int Width, int Height, int Current, int Total);
};

cDvdProgressBar::cDvdProgressBar(int Width, int Height, int Current, int Total)
:cBitmap(Width, Height, 2)
{
    total = Total;
    if (total > 0) {
        int p = Pos(Current);
        Fill(0, 0, p, Height - 1, clrGreen);
        Fill(p + 1, 0, Width - 1, Height - 1, clrWhite);
    }
}

#endif

// --- cDvdPlayerControl -----------------------------------------------------

cDvdPlayerControl::cDvdPlayerControl(void)
    :cControl(player = new cDvdPlayer())
{
    assert(dvd_active == false);
    dvd_active = true;
    visible = modeOnly = shown = displayFrames = false;
    lastCurrent = lastTotal = -1;

#if VDRVERSNUM<10307
    osdPos=0;
    osdPosOffsetSet = false;
    osdPosOffset    = 0;
#else
    displayReplay=NULL;
#endif
    timeoutShow = 0;
    inputActive = NoneInput;
    inputHide=true;
    forceDvdNavigation=false;

    player->setController(this);

    cStatus::MsgReplaying(this, "DVD");
}

cDvdPlayerControl::~cDvdPlayerControl()
{
    Hide();
    cStatus::MsgReplaying(this, NULL);
    Stop();
    assert(dvd_active == true);
    dvd_active = false;
}

bool cDvdPlayerControl::Active(void)
{
    return player && player->Active();
}

void cDvdPlayerControl::Stop(void)
{
    if(player) {
        player->Stop();
        delete player;
        player=NULL;
    }
}

void cDvdPlayerControl::Pause(void)
{
    if (player)
        player->Pause();
}

void cDvdPlayerControl::Play(void)
{
    if (player)
        player->Play();
}

void cDvdPlayerControl::Forward(void)
{
    if (player)
        player->Forward();
}

void cDvdPlayerControl::Backward(void)
{
    if (player)
        player->Backward();
}

void cDvdPlayerControl::SkipSeconds(int Seconds)
{
    if (player)
        player->SkipSeconds(Seconds);
}

int cDvdPlayerControl::SkipFrames(int Frames)
{
    if (player)
        return player->SkipFrames(Frames);
  return -1;
}

bool cDvdPlayerControl::GetIndex(int &Current, int &Total, bool SnapToIFrame)
{
    if (player) {
        player->GetIndex(Current, Total, SnapToIFrame);
        return true;
    }
    Current=-1; Total=-1;
    return false;
}

bool cDvdPlayerControl::GetReplayMode(bool &Play, bool &Forward, int &Speed)
{
    return player && player->GetReplayMode(Play, Forward, Speed);
}

void cDvdPlayerControl::Goto(int Seconds, bool Still)
{
    if (player)
    {
        player->Goto(Seconds, Still);
    }
}

void cDvdPlayerControl::OsdClose()
{
#if VDRVERSNUM<10307
    DEBUG_OSDPOS("*** OSD Close\n");
    if( osdPosOffsetSet ) {
        osdPos -= osdPosOffset;
        osdPosOffset    = 0;
        osdPosOffsetSet = false;
    DEBUG_OSDPOS("*** OSD POS RESET: osdPos=%d\n", osdPos);
    }
    Interface->Close();
#else
    delete displayReplay;
    displayReplay=NULL;
#endif
}

#if VDRVERSNUM<10307
void cDvdPlayerControl::OsdOpen(int width, int height)
{
    DEBUG_OSDPOS("*** OSD Open\n");
    if(!osdPosOffsetSet && player!=NULL) {
#if VDRVERSNUM>=10300
        int h = cFont::GetFont(fontOsd)->Height('X');
#else
        cFont font(fontOsd);
        int h = font.Height('X');
#endif
        osdPosOffset = ( player->getHeight()/h - Setup.OSDheight ) - 3 ;
        osdPosOffsetSet = true;
        osdPos += osdPosOffset;
        height += osdPosOffset;
        DEBUG_OSDPOS("*** OSD POS SET: video_h=%d, font_h=%d, osd_l=%d, offset_l=%d, osdPos=%d, height=%d\n",
            player->getHeight(), h, Setup.OSDheight, osdPosOffset, osdPos, height);
    }

    Interface->Open(width, height);
#else
void cDvdPlayerControl::OsdOpen(void)
{
    displayReplay=Skins.Current()->DisplayReplay(modeOnly);
#endif
}

void cDvdPlayerControl::ShowTimed(int Seconds) {
    if (modeOnly)
        Hide();
    if (!visible) {
        shown = ShowProgress(true);
        timeoutShow = (shown && Seconds > 0) ? time(NULL) + Seconds : 0;
    }
}

void cDvdPlayerControl::Show(void) {
    ShowTimed();
}

void cDvdPlayerControl::Hide(void)
{
    if (visible) {
        OsdClose();
        needsFastResponse = visible = false;
        modeOnly = false;
    }
}

void cDvdPlayerControl::DisplayAtBottom(const char *s)
{
#if VDRVERSNUM<10307
    const int p=modeOnly ? 0 : 2;
    if (s) {
        int w = cOsd::WidthInCells(s);
        int d = max(Width() - w, 0) / 2;
        if(modeOnly) Interface->Fill(0, p, Interface->Width(), 1, clrTransparent);
        Interface->Write(d, p, s);
        Interface->Flush();
    } else
        Interface->Fill(12, p, Width() - 22, 1, clrBackground);
#else
    displayReplay->SetJump(s);
#endif
}

void cDvdPlayerControl::ShowMode(void)
{
  if (Setup.ShowReplayMode && inputActive==NoneInput ) {
     bool Play, Forward;
     int Speed;
     if (GetReplayMode(Play, Forward, Speed)) {
        bool NormalPlay = (Play && Speed == -1);

        if (!visible) {
           if (NormalPlay)
              return; // no need to do indicate ">" unless there was a different mode displayed before
           visible = modeOnly = true;
           // open small display
#if VDRVERSNUM<10307
           OsdOpen(0, osdPos-1); //XXX remove when displaying replay mode differently
#else
           OsdOpen();
#endif
        }
        if (modeOnly && !timeoutShow && NormalPlay)
           timeoutShow = time(NULL) + MODETIMEOUT;
#if VDRVERSNUM<10307
        const char *Mode;
        if (Speed == -1) Mode = Play    ? "  >  " : " ||  ";
        else if (Play)   Mode = Forward ? " X>> " : " <<X ";
        else             Mode = Forward ? " X|> " : " <|X ";
        char buf[16];
        strn0cpy(buf, Mode, sizeof(buf));
        char *p = strchr(buf, 'X');
        if (p)
           *p = Speed > 0 ? '1' + Speed - 1 : ' ';

        eDvbFont OldFont = Interface->SetFont(fontFix);
        DisplayAtBottom(buf);
        Interface->SetFont(OldFont);
#else
        displayReplay->SetMode(Play, Forward, Speed);

#endif
        }
    }
}

const char * cDvdPlayerControl::GetDisplayHeaderLine()
{
    const char * titleinfo_str=NULL;
    const char * aspect_str=NULL;
    const char * audiolang_str=NULL;
    const char * spulang_str=NULL;
    const char * title_str=NULL;
    static char title_buffer[256];

    title_buffer[0]=0;
    if(!player) return title_buffer;

    player->GetTitleInfoString( &titleinfo_str );
    player->GetAudioLangCode( &audiolang_str );
    player->GetSubpLangCode( &spulang_str );
    player->GetAspectString( &aspect_str );
    player->GetTitleString( &title_str );

    snprintf(title_buffer, 255, "%s, %s, %s, %s, %s    ",
        titleinfo_str, audiolang_str, spulang_str, aspect_str, title_str);

    return title_buffer;
}

bool cDvdPlayerControl::ShowProgress(bool Initial)
{
    int Current, Total;
    const char * title_buffer=NULL;
    static char last_title_buffer[256];

    GetIndex(Current, Total);

    if (Total > 0) {
        if (!visible) {
            needsFastResponse = visible = true;
#if VDRVERSNUM<10307
            OsdOpen(Setup.OSDwidth, osdPos-3);
#else
            OsdOpen();
#endif
        }

        if (Initial) {
#if VDRVERSNUM<10307
            Interface->Clear();
            if(osdPos<0) Interface->Fill(0,3,Interface->Width(),-osdPos,clrTransparent);
            lastCurrent = lastTotal = -1;
            last_title_buffer[0]=0;
            Interface->Write(0, 0, "unknown title");
#else
            lastCurrent = lastTotal = -1;
            last_title_buffer[0]=0;
            displayReplay->SetTitle("unknown title");
#endif
            cStatus::MsgReplaying(this, "unknown title");
        }

        if (player) {
            title_buffer = GetDisplayHeaderLine();
            if ( strcmp(title_buffer,last_title_buffer) != 0 ) {
#if VDRVERSNUM<10307
                Interface->Write(0, 0, title_buffer);
                if (!Initial)
                    Interface->Flush();
#else
                displayReplay->SetTitle(title_buffer);
                if (!Initial) displayReplay->Flush();
#endif
                cStatus::MsgReplaying(this, title_buffer);
                strcpy(last_title_buffer, title_buffer);
            }
        }

#if VDRVERSNUM<10307
        if (Total != lastTotal) {
            Interface->Write(-7, 2, IndexToHMSF(Total));
            if (!Initial)
                Interface->Flush();
        }
        if (Current != lastCurrent || Total != lastTotal) {
#ifdef DEBUG_OSD
            int p = Width() * Current / Total;
            Interface->Fill(0, 1, p, 1, clrGreen);
            Interface->Fill(p, 1, Width() - p, 1, clrWhite);
#else
            cDvdProgressBar ProgressBar(Width() * cOsd::CellWidth(), cOsd::LineHeight(), Current, Total);
            Interface->SetBitmap(0, cOsd::LineHeight(), ProgressBar);
            if (!Initial)
                Interface->Flush();
#endif
            Interface->Write(0, 2, IndexToHMSF(Current, displayFrames));
            Interface->Flush();
            lastCurrent = Current;
        }
#else
        if (Current != lastCurrent || Total != lastTotal) {
            displayReplay->SetProgress(Current, Total);
            displayReplay->SetTotal(IndexToHMSF(Total));
            displayReplay->SetCurrent(IndexToHMSF(Current, displayFrames));
            lastCurrent = Current;
            if (!Initial) displayReplay->Flush();
        }
#endif
        lastTotal = Total;
        ShowMode();
        return true;
    }
    return false;
}

void cDvdPlayerControl::InputIntDisplay(const char * msg, int val)
{
    char buf[120];
    snprintf(buf,sizeof(buf),"%s %d", msg, val);
    DisplayAtBottom(buf);
}

void cDvdPlayerControl::InputIntProcess(eKeys Key, const char * msg, int & val)
{
    switch (Key) {
        case k0 ... k9:
            val = val * 10 + Key - k0;
            InputIntDisplay(msg, val);
            break;
        case kOk:
        case kLeft:
        case kRight:
        case kUp:
        case kDown:
            switch ( inputActive ) {
                case TimeSearchInput:
                    break;
                case TrackSearchInput:
                    if(player) player->GotoTitle(val);
                    break;
                default:
                    break;
            }
            inputActive = NoneInput;
            break;
        default:
            inputActive = NoneInput;
            break;
    }

    if (inputActive==NoneInput) {
        if (inputHide)
            Hide();
        else
            DisplayAtBottom();
        ShowMode();
    }
}

void cDvdPlayerControl::TrackSearch(void)
{
    inputIntVal=0;
    inputIntMsg="Track: ";

    inputHide = false;

    if (modeOnly)
        Hide();
    if (!visible) {
        Show();
        if (visible)
            inputHide = true;
        else
            return;
    }
    timeoutShow = 0;
    InputIntDisplay(inputIntMsg, inputIntVal);
    inputActive = TrackSearchInput;
}

void cDvdPlayerControl::TimeSearchDisplay(void)
{
    char buf[64];
    strcpy(buf, tr("Jump: "));
    int len = strlen(buf);
    char h10 = '0' + (timeSearchTime >> 24);
    char h1  = '0' + ((timeSearchTime & 0x00FF0000) >> 16);
    char m10 = '0' + ((timeSearchTime & 0x0000FF00) >> 8);
    char m1  = '0' + (timeSearchTime & 0x000000FF);
    char ch10 = timeSearchPos > 3 ? h10 : '-';
    char ch1  = timeSearchPos > 2 ? h1  : '-';
    char cm10 = timeSearchPos > 1 ? m10 : '-';
    char cm1  = timeSearchPos > 0 ? m1  : '-';
    sprintf(buf + len, "%c%c:%c%c", ch10, ch1, cm10, cm1);
    DisplayAtBottom(buf);
}

void cDvdPlayerControl::TimeSearchProcess(eKeys Key)
{
#define STAY_SECONDS_OFF_END 10
    int Seconds = (timeSearchTime >> 24) * 36000 + ((timeSearchTime & 0x00FF0000) >> 16) * 3600 + ((timeSearchTime & 0x0000FF00) >> 8) * 600 + (timeSearchTime & 0x000000FF) * 60;
    int Current = (lastCurrent / FRAMESPERSEC);
    int Total = (lastTotal / FRAMESPERSEC);
    switch (Key) {
        case k0 ... k9:
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
            switch ( inputActive ) {
                case TimeSearchInput:
                    SkipSeconds(Seconds * dir);
                    break;
                case TrackSearchInput:
                    break;
                default:
                    break;
            }
            inputActive = NoneInput;
            break;
        }
        case kUp:
        case kDown:
            Seconds = min(Total - STAY_SECONDS_OFF_END, Seconds);
            switch ( inputActive ) {
                case TimeSearchInput:
                    Goto(Seconds, Key == kDown);
                    break;
                case TrackSearchInput:
                    break;
                default:
                    break;
            }
            inputActive = NoneInput;
            break;
        default:
            inputActive = NoneInput;
            break;
    }

    if (inputActive==NoneInput) {
        if (inputHide)
            Hide();
        else
            DisplayAtBottom();
        ShowMode();
    }
}

void cDvdPlayerControl::TimeSearch(void)
{
    timeSearchTime = timeSearchPos = 0;
    inputHide = false;
    if (modeOnly)
        Hide();
    if (!visible) {
        Show();
        if (visible)
            inputHide = true;
        else
            return;
    }
    timeoutShow = 0;
    TimeSearchDisplay();
    inputActive = TimeSearchInput;
}

bool cDvdPlayerControl::DvdNavigation(eKeys Key)
{
    if (!player)
    return false;

    Hide();
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

eOSState cDvdPlayerControl::ProcessKey(eKeys Key)
{
    if (!Active())
        return osEnd;
    if (visible ) {
        if (timeoutShow && time(NULL) > timeoutShow) {
            Hide();
            ShowMode();
            timeoutShow = 0;
        }
        else if (modeOnly)
            ShowMode();
        else
            shown = ShowProgress(!shown) || shown;
    }
    if (!visible ) {
        const char * title_buffer=NULL;
        static char last_title_buffer[256];

        if (player) {
            title_buffer = GetDisplayHeaderLine();
            if ( strcmp(title_buffer,last_title_buffer) != 0 ) {
                strcpy(last_title_buffer, title_buffer);
                cStatus::MsgReplaying(this, title_buffer);
            }
        }
    }

    bool DisplayedFrames = displayFrames;
    displayFrames = false;
    if (inputActive!=NoneInput && Key != kNone) {
        switch ( inputActive ) {
            case TimeSearchInput:
                TimeSearchProcess(Key);
                break;
            case TrackSearchInput:
                InputIntProcess(Key, inputIntMsg, inputIntVal);
                break;
            default:
                break;
        }
        return osContinue;
    }
    bool DoShowMode = true;

    bool done = true;
    if ( ( player && player->IsInMenuDomain() ) || forceDvdNavigation ) {

        switch (Key) {
            case kRed: forceDvdNavigation=false; break;
            case kGreen|k_Repeat:
            case kGreen:   player->PreviousTitle(); break;
            case kYellow|k_Repeat:
            case kYellow:  player->NextTitle(); break;

            case kUp:
            case kDown:
            case kLeft:
            case kRight:
            case kOk:
            case k0 ... k9:
                DoShowMode = false;
                displayFrames = DisplayedFrames;
                if (DvdNavigation(Key))
                    break;
            default:
                done = false;
            break;
        }
    } else {
        switch (Key) {
            // Positioning:
            case kPlay:
            case kUp:      Play(); break;

            case kPause:
            case kDown:    Pause(); break;

            case kFastRew|k_Release:
            case kLeft|k_Release:
                if (Setup.MultiSpeedMode) break;
            case kFastRew:
            case kLeft:    Backward(); break;

            case kFastFwd|k_Release:
            case kRight|k_Release:
                if (Setup.MultiSpeedMode) break;
            case kFastFwd:
            case kRight:   Forward(); break;

            default:
                done = false;
                break;
        }
    }

    if (!done) {
        switch (Key) {
            // Positioning:
            case kRed:     TimeSearch(); break;
            case kGreen|k_Repeat:
            case kGreen:   SkipSeconds(-60); break;
            case kYellow|k_Repeat:
            case kYellow:  SkipSeconds( 60); break;
            case kBlue:    TrackSearch(); break;

            default: {
                DoShowMode = false;
                displayFrames = DisplayedFrames;
                switch (Key) {
                    // Menu control:
                    case kOk:
                        if (visible && !modeOnly ) {
                            Hide();
                            DoShowMode = true;
                        } else
                            Show();
                        break;

                    case kStop:
                    case kBack:
                        Hide();
                        Stop();
                        return osEnd;

                    case k1: if(player) {
                        bool wasVisible = visible;
                        if(wasVisible) Hide();
                        player->NextAudioID();
                        if(wasVisible) { usleep(200000); Show(); }
                        break;
                    }
                    case k2: if(player) {
                        player->NextSubpStream();
                        if ( player->GetCurrentNavSubpStream()==-1 )
                        {
                            Hide();
                            Show();
                        }
                    }
                    break;
                    case k3: if(player) player->NextAngle(); break;

                    case k4: if(player) player->PreviousPart(); break;
                    case k6: if(player) player->NextPart(); break;

                    case kChanDn:
                    case k7: if(player) player->PreviousTitle(); break;

                    case kChanUp:
                    case k9: if(player) player->NextTitle(); break;

                    case k5:
                        if(visible && !modeOnly ) {
                            Hide();
                            player->callRootMenu();
                        } else {
                            Hide();
                            forceDvdNavigation=true;
                        }
                        break;

                    case k8:
                        if(visible && !modeOnly ) {
                            Hide();
#if VDRVERSNUM<10307
                            osdPos--; if(osdPos<-6) osdPos=-6;
#endif
                            Show();
                        } else {
                            Hide();
                            player->callTitleMenu();
                        }
                        break;
                    case k0:
                        if(visible && !modeOnly ) {
                            Hide();
#if VDRVERSNUM<10307
                            osdPos++; if(osdPos>0) osdPos=0;
#endif
                            Show();
                        } else {
                            Hide();
                            player->callAudioMenu();
                        }
                        break;

                    default:
                        return osUnknown;
                }
            }
        }
    }
    if (DoShowMode)
        ShowMode();
#if VDRVERSNUM<10307
    if (DisplayedFrames && !displayFrames)
        Interface->Fill(0, 2, 11, 1, clrBackground);
#endif
    return osContinue;
}
