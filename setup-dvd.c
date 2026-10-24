/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. Read the file COPYING for details.
 */

#include <vdr/menuitems.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "setup-dvd.h"
#include "i18n.h"
#ifdef __QNXNTO__
#include <strings.h>
#endif

cDVDSetup DVDSetup;

// --- cDVDSetup -----------------------------------------------------------

cDVDSetup::cDVDSetup(void)
{
    MenuLanguage  = 0;
    AudioLanguage = 0;
    SpuLanguage   = 0;
    PlayerRCE     = 2;
    ShowSubtitles = 0;
    HideMainMenu  = 0;
    ReadAHead     = 0;
    Gain          = 4;

    AC3dynrng     = 0;
}

// Value if it is one the setup menu offers, else Default: the languages
// index ISO639code, the region code is a shift count.
static int SetupValue(const char *Value, int Min, int Max, int Default)
{
    int v = atoi(Value);
    return Min <= v && v <= Max ? v : Default;
}

bool cDVDSetup::SetupParse(const char *Name, const char *Value)
{
    // Parse your own setup parameters and store their values.
    if      (!strcasecmp(Name, "MenuLanguage"))  MenuLanguage  = SetupValue(Value, 0, ISO639codes - 1, MenuLanguage);
    else if (!strcasecmp(Name, "AudioLanguage")) AudioLanguage = SetupValue(Value, 0, ISO639codes - 1, AudioLanguage);
    else if (!strcasecmp(Name, "SpuLanguage"))   SpuLanguage   = SetupValue(Value, 0, ISO639codes - 1, SpuLanguage);
    else if (!strcasecmp(Name, "PlayerRCE"))     PlayerRCE     = SetupValue(Value, 0, 8, PlayerRCE);
    else if (!strcasecmp(Name, "ShowSubtitles")) ShowSubtitles = atoi(Value);
    else if (!strcasecmp(Name, "HideMainMenu"))  HideMainMenu  = atoi(Value);
    else if (!strcasecmp(Name, "ReadAHead"))     ReadAHead     = atoi(Value);
    else if (!strcasecmp(Name, "Gain"))          Gain          = atoi(Value);
    else
	return false;
    return true;
}

// --- cMenuSetupDVD --------------------------------------------------------

cMenuSetupDVD::cMenuSetupDVD(void)
{
    data = DVDSetup;
    SetSection(tr("Plugin.DVD$DVD"));
#if VDRVERSNUM >= 10507
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred menu language"),     &data.MenuLanguage,  ISO639codes, ISO639code));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred audio language"),    &data.AudioLanguage, ISO639codes, ISO639code));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred subtitle language"), &data.SpuLanguage,   ISO639codes, ISO639code));
#else
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred menu language"),     &data.MenuLanguage,  I18nNumLanguages, I18nLanguages()));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred audio language"),    &data.AudioLanguage, I18nNumLanguages, I18nLanguages()));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred subtitle language"), &data.SpuLanguage,   I18nNumLanguages, I18nLanguages()));
#endif
    Add(new cMenuEditIntItem( tr("Setup.DVD$Player region code"),          &data.PlayerRCE, 0, 8));
    Add(new cMenuEditBoolItem(tr("Setup.DVD$Display subtitles"),           &data.ShowSubtitles));
    Add(new cMenuEditBoolItem(tr("Setup.DVD$Hide Mainmenu Entry"),         &data.HideMainMenu));
    Add(new cMenuEditBoolItem(tr("Setup.DVD$ReadAHead"),                   &data.ReadAHead));
    Add(new cMenuEditIntItem( tr("Setup.DVD$Gain (analog)"),               &data.Gain, 0, 10));
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
    SetupStore("Gain",          DVDSetup.Gain  );
}




