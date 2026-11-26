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
    int MenuLanguage;
    int AudioLanguage;
    int SpuLanguage;
    int PlayerRCE;
    int ShowSubtitles;
    int HideMainMenu;
    int ReadAHead;

public:
    cDVDSetup(void);

    bool SetupParse(const char *Name, const char *Value);
};

class cMenuSetupDVD : public cMenuSetupPage {
private:
    cDVDSetup data;

protected:
    void Store(void) override;

public:
    cMenuSetupDVD(void);
};

extern cDVDSetup DVDSetup;

extern const char *ISO639code[];
extern const char *DvdLanguageName[]; // of each code
extern const int ISO639codes;

#endif // VDR_DVD_SETUP_DVD_H
