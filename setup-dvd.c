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

cDVDSetup DVDSetup;

// The DVD stores a two-letter ISO 639-1 language code with each stream. These
// are the codes offered as the preferred languages and passed to libdvdnav,
// plus the language names shown in VDR's menus (ASCII). VDR's own language
// handling uses three-letter codes and does not cover this.
const char *ISO639code[] = {"en", "de", "sl", "it", "nl", "pt", "fr", "no", "fi", "pl", "es",
                            "el", "sv", "ro", "hu", "ca", "ru", "hr", "et", "da", "cs", "tr"};
const int ISO639codes = sizeof(ISO639code) / sizeof(ISO639code[0]);

const char *DvdLanguageCode[][2] = {{"en", "English"},    {"de", "Deutsch"},   {"sl", "Slovenski"}, {"it", "Italiano"},
                                    {"nl", "Nederlands"}, {"pt", "Portugues"}, {"fr", "Francais"},  {"no", "Norsk"},
                                    {"fi", "Suomi"},      {"pl", "Polski"},    {"es", "Espanol"},   {"el", "Greek"},
                                    {"sv", "Svenska"},    {"ro", "Romaneste"}, {"hu", "Magyar"},    {"ca", "Catala"},
                                    {"ru", "Russian"},    {"hr", "Hrvatski"},  {"et", "Eesti"},     {"da", "Dansk"},
                                    {"cs", "Czech"},      {"tr", "Turkce"}};
const int DvdLanguageCodes = sizeof(DvdLanguageCode) / sizeof(DvdLanguageCode[0]);

// --- cDVDSetup -----------------------------------------------------------

cDVDSetup::cDVDSetup(void)
{
    MenuLanguage = 0;
    AudioLanguage = 0;
    SpuLanguage = 0;
    PlayerRCE = 2;
    ShowSubtitles = 0;
    HideMainMenu = 0;
    ReadAHead = 0;
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
    if (!strcasecmp(Name, "MenuLanguage"))
        MenuLanguage = SetupValue(Value, 0, ISO639codes - 1, MenuLanguage);
    else if (!strcasecmp(Name, "AudioLanguage"))
        AudioLanguage = SetupValue(Value, 0, ISO639codes - 1, AudioLanguage);
    else if (!strcasecmp(Name, "SpuLanguage"))
        SpuLanguage = SetupValue(Value, 0, ISO639codes - 1, SpuLanguage);
    else if (!strcasecmp(Name, "PlayerRCE"))
        PlayerRCE = SetupValue(Value, 0, 8, PlayerRCE);
    else if (!strcasecmp(Name, "ShowSubtitles"))
        ShowSubtitles = atoi(Value);
    else if (!strcasecmp(Name, "HideMainMenu"))
        HideMainMenu = atoi(Value);
    else if (!strcasecmp(Name, "ReadAHead"))
        ReadAHead = atoi(Value);
    else if (!strcasecmp(Name, "Gain"))
        ; // removed with the AC-3 decoder; ignored, or VDR logs it as an error
    else
        return false;
    return true;
}

// --- cMenuSetupDVD --------------------------------------------------------

cMenuSetupDVD::cMenuSetupDVD(void)
{
    data = DVDSetup;
    SetSection(tr("Plugin.DVD$DVD"));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred menu language"), &data.MenuLanguage, ISO639codes, ISO639code));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred audio language"), &data.AudioLanguage, ISO639codes, ISO639code));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred subtitle language"), &data.SpuLanguage, ISO639codes, ISO639code));
    Add(new cMenuEditIntItem(tr("Setup.DVD$Player region code"), &data.PlayerRCE, 0, 8));
    Add(new cMenuEditBoolItem(tr("Setup.DVD$Display subtitles"), &data.ShowSubtitles));
    Add(new cMenuEditBoolItem(tr("Setup.DVD$Hide Mainmenu Entry"), &data.HideMainMenu));
    Add(new cMenuEditBoolItem(tr("Setup.DVD$ReadAHead"), &data.ReadAHead));
}

void cMenuSetupDVD::Store(void)
{
    DVDSetup = data;
    SetupStore("MenuLanguage", DVDSetup.MenuLanguage);
    SetupStore("AudioLanguage", DVDSetup.AudioLanguage);
    SetupStore("SpuLanguage", DVDSetup.SpuLanguage);
    SetupStore("PlayerRCE", DVDSetup.PlayerRCE);
    SetupStore("ShowSubtitles", DVDSetup.ShowSubtitles);
    SetupStore("HideMainMenu", DVDSetup.HideMainMenu);
    SetupStore("ReadAHead", DVDSetup.ReadAHead);
}
