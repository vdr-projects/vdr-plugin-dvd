/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the 
 * GNU GENERAL PUBLIC LICENSE. Read the file COPYING for details.
 */

#include <vdr/menuitems.h>

#include "setup-dvd.h"
#include "i18n.h"

cDVDSetup DVDSetup;

// --- cDVDSetup -----------------------------------------------------------

cDVDSetup::cDVDSetup(void)
{
    MenuLanguage  = 0;
    AudioLanguage = 0;
    SpuLanguage   = 0;
    PlayerRCE     = 2;
    ShowSubtitles = 2;
    HideMainMenu  = 0;
    ReadAHead     = 0;
    
    AC3dynrng  = 0;
}

bool cDVDSetup::SetupParse(const char *Name, const char *Value)
{
    // Parse your own setup parameters and store their values.
    if      (!strcasecmp(Name, "MenuLanguage"))  MenuLanguage  = atoi(Value);
    else if (!strcasecmp(Name, "AudioLanguage")) AudioLanguage = atoi(Value);
    else if (!strcasecmp(Name, "SpuLanguage"))   SpuLanguage   = atoi(Value);
    else if (!strcasecmp(Name, "PlayerRCE"))     PlayerRCE     = atoi(Value);
    else if (!strcasecmp(Name, "ShowSubtitles")) ShowSubtitles = atoi(Value);
    else if (!strcasecmp(Name, "HideMainMenu"))  HideMainMenu  = atoi(Value);
    else if (!strcasecmp(Name, "ReadAHead"))     ReadAHead     = atoi(Value);
    else
	return false;
    return true;
}

// --- cMenuSetupDVD --------------------------------------------------------

cMenuSetupDVD::cMenuSetupDVD(void)
{
    spuOptionsText[0] = tr("never");
    spuOptionsText[1] = tr("always");
    spuOptionsText[2] = tr("Setup.DVD$forced only");

    data = DVDSetup;
    SetSection(tr("DVD"));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred menu language"),     &data.MenuLanguage,  I18nNumLanguages, I18nLanguages()));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred audio language"),    &data.AudioLanguage, I18nNumLanguages, I18nLanguages()));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred subtitle language"), &data.SpuLanguage,   I18nNumLanguages, I18nLanguages()));
    Add(new cMenuEditIntItem( tr("Setup.DVD$Player region code"),         &data.PlayerRCE));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Display subtitles"),           &data.ShowSubtitles, 3, spuOptionsText));
    Add(new cMenuEditBoolItem(tr("Setup.DVD$Hide Mainmenu Entry"),        &data.HideMainMenu));
    Add(new cMenuEditIntItem( tr("Setup.DVD$ReadAHead"),                  &data.ReadAHead));

}

void cMenuSetupDVD::Store(void)
{
    DVDSetup = data;
    SetupStore("MenuLanguage",  DVDSetup.MenuLanguage  );
    SetupStore("AudioLanguage", DVDSetup.AudioLanguage );
    SetupStore("SpuLanguage",   DVDSetup.SpuLanguage   );
    SetupStore("PlayerRCE",     DVDSetup.PlayerRCE     );
    SetupStore("ShowSubtitles", DVDSetup.ShowSubtitles );
    SetupStore("HideMainMenu",  DVDSetup.HideMainMenu  );
    SetupStore("ReadAHead",     DVDSetup.ReadAHead  );
}




