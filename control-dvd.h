/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#ifndef VDR_DVD_CONTROL_DVD_H
#define VDR_DVD_CONTROL_DVD_H

// C++ Standard Library
#include <atomic>
#include <ctime>

// VDR
#include <vdr/keys.h>
#include <vdr/osdbase.h>
#include <vdr/player.h>
#include <vdr/skins.h>
#include <vdr/tools.h>

class cDvdPlayer;

enum InputProcessType { NoneInput, TimeSearchInput, TrackSearchInput };

class cDvdPlayerControl : public cControl {
    friend class cDvdPlayer;

  private:
    static bool dvd_active;
    bool displayFrames;
    bool forceDvdNavigation;
    bool inputHide;
    bool lastForward;
    bool lastPlay;
    bool modeOnly;
    bool shown;
    std::atomic<bool> visible;    // the player thread asks OsdVisible() for each SPU
    std::atomic<void *> osdTaker; // the player thread takes the OSD for the SPU
    int inputIntVal;
    int lastCurrent;
    int lastTotal;
    int lastSpeed;
    int timeSearchPos;
    int timeSearchTime;
    cString shownTitle;    // in the replay display
    cString reportedTitle; // to cStatus while the display is closed
    InputProcessType inputActive;
    time_t timeoutShow;
    const char *inputIntMsg;
    void InputIntDisplay(const char *msg, int val);
    void InputIntProcess(eKeys Key, const char *msg, int &val);
    auto IsDvdNavigationForced() -> bool { return forceDvdNavigation; }
    void OsdOpen();
    void OsdClose();
    void ShowTimed(int Seconds = 0);
    void ShowMode();
    auto ShowProgress(bool Initial) -> bool;
    void TimeSearchDisplay();
    void TimeSearchProcess(eKeys Key);
    void TimeSearch();
    void TrackSearch();
    void UpdateShow(bool force = false);
    // cControl owns the player, the control keeps its type.
    explicit cDvdPlayerControl(cDvdPlayer *Player);

  public:
    cDvdPlayerControl();
    ~cDvdPlayerControl() override;
    void Hide() override;
    auto ProcessKey(eKeys Key) -> eOSState override;
    void Show() override;
    static auto DVDActive() -> bool { return dvd_active; }
    auto DvdNavigation(eKeys Key) -> bool;
    auto GetDisplayHeaderLine() -> cString;
    auto OsdVisible(const void *me) -> bool;
    auto OsdTaken(const void *me) -> bool;
    auto TakeOsd(void *obj) -> bool;

  protected:
    void HideOwnOsd();

  private:
    cDvdPlayer *player; // cControl's, never NULL
    cSkinDisplayReplay *displayReplay;

  public:
    auto Active() -> bool;
    void Stop();
    // Stops the current replay session (if any).
    void Pause();
    // Pauses the current replay session, or resumes a paused session.
    void Play();
    // Resumes normal replay mode.
    void Forward();
    // Runs the current replay session forward at a higher speed.
    void Backward();
    // Runs the current replay session backwards at a higher speed.
    void SkipSeconds(int Seconds);
    // Skips the given number of seconds in the current replay session.
    // The sign of 'Seconds' determines the direction in which to skip.
    // Use a very large negative value to go all the way back to the
    // beginning of the program chain.
    void Goto(int Seconds);
    // Positions to the given second of the current program chain.
};

#endif // VDR_DVD_CONTROL_DVD_H
