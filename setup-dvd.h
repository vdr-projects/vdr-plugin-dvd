/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#ifndef VDR_DVD_SETUP_DVD_H
#define VDR_DVD_SETUP_DVD_H

// VDR
#include <vdr/menuitems.h>

class cDVDSetup {
  public:
    int MenuLanguage{0};
    int AudioLanguage{0};
    int SpuLanguage{0};
    int PlayerRCE{2};
    int ShowSubtitles{0};
    int HideMainMenu{0};
    int ReadAHead{0};

    auto SetupParse(const char *Name, const char *Value) -> bool;
};

class cMenuSetupDVD : public cMenuSetupPage {
  private:
    cDVDSetup data;

  protected:
    void Store() override;

  public:
    cMenuSetupDVD();
};

extern cDVDSetup DVDSetup;

extern const char *ISO639code[];
extern const char *DvdLanguageName[]; // of each code
extern const int ISO639codes;

#endif // VDR_DVD_SETUP_DVD_H
