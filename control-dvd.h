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
    bool IsDvdNavigationForced(void) { return forceDvdNavigation; }
    void OsdOpen(void);
    void OsdClose();
    void ShowTimed(int Seconds = 0);
    void ShowMode(void);
    bool ShowProgress(bool Initial);
    void TimeSearchDisplay(void);
    void TimeSearchProcess(eKeys Key);
    void TimeSearch(void);
    void TrackSearch(void);
    void UpdateShow(bool force = false);

public:
    cDvdPlayerControl(void);
    ~cDvdPlayerControl() override;
    void Hide(void) override;
    eOSState ProcessKey(eKeys Key) override;
    void Show(void) override;
    static bool DVDActive() { return dvd_active; };
    bool DvdNavigation(eKeys Key);
    cString GetDisplayHeaderLine(void);
    bool OsdVisible(const void *me);
    bool OsdTaken(const void *me);
    bool TakeOsd(void *obj);

protected:
    void HideOwnOsd(void);

private:
    cDvdPlayer *player; // cControl's, never NULL
    cSkinDisplayReplay *displayReplay;

public:
    bool Active(void);
    void Stop(void);
    // Stops the current replay session (if any).
    void Pause(void);
    // Pauses the current replay session, or resumes a paused session.
    void Play(void);
    // Resumes normal replay mode.
    void Forward(void);
    // Runs the current replay session forward at a higher speed.
    void Backward(void);
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
