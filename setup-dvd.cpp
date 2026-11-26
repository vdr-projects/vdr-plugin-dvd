/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. Read the file COPYING for details.
 */

#include "setup-dvd.h"

// C++ Standard Library
#include <cerrno>
#include <cstdlib>

// POSIX
#include <strings.h>

// VDR
#include <vdr/i18n.h>

cDVDSetup DVDSetup;

// The DVD stores a two-letter ISO 639-1 language code with each stream. These
// are the codes offered as the preferred languages and passed to libdvdnav,
// plus the language names shown in VDR's menus (ASCII). VDR's own language
// handling uses three-letter codes and does not cover this.
const char *ISO639code[] = {"en", "de", "sl", "it", "nl", "pt", "fr", "no", "fi", "pl", "es",
                            "el", "sv", "ro", "hu", "ca", "ru", "hr", "et", "da", "cs", "tr"};
const int ISO639codes = static_cast<int>(sizeof(ISO639code) / sizeof(ISO639code[0]));

const char *DvdLanguageName[] = {"English",  "Deutsch",   "Slovenski", "Italiano", "Nederlands", "Portugues",
                                 "Francais", "Norsk",     "Suomi",     "Polski",   "Espanol",    "Greek",
                                 "Svenska",  "Romaneste", "Magyar",    "Catala",   "Russian",    "Hrvatski",
                                 "Eesti",    "Dansk",     "Czech",     "Turkce"};
static_assert(sizeof(DvdLanguageName) == sizeof(ISO639code), "a name for each code");

// --- cDVDSetup -----------------------------------------------------------

cDVDSetup::cDVDSetup() {
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
static auto SetupValue(const char *Value, int Min, int Max, int Default) -> int {
    char *end;
    errno = 0;
    const long v = strtol(Value, &end, 10);
    return end != Value && !*end && !errno && Min <= v && v <= Max ? static_cast<int>(v) : Default;
}

auto cDVDSetup::SetupParse(const char *Name, const char *Value) -> bool {
    if (!strcasecmp(Name, "MenuLanguage")) {
        MenuLanguage = SetupValue(Value, 0, ISO639codes - 1, MenuLanguage);
    } else if (!strcasecmp(Name, "AudioLanguage")) {
        AudioLanguage = SetupValue(Value, 0, ISO639codes - 1, AudioLanguage);
    } else if (!strcasecmp(Name, "SpuLanguage")) {
        SpuLanguage = SetupValue(Value, 0, ISO639codes - 1, SpuLanguage);
    } else if (!strcasecmp(Name, "PlayerRCE")) {
        PlayerRCE = SetupValue(Value, 0, 8, PlayerRCE);
    } else if (!strcasecmp(Name, "ShowSubtitles")) {
        ShowSubtitles = SetupValue(Value, 0, 1, ShowSubtitles);
    } else if (!strcasecmp(Name, "HideMainMenu")) {
        HideMainMenu = SetupValue(Value, 0, 1, HideMainMenu);
    } else if (!strcasecmp(Name, "ReadAHead")) {
        ReadAHead = SetupValue(Value, 0, 1, ReadAHead);
    } else if (!strcasecmp(Name, "Gain")) {
        ; // removed with the AC-3 decoder; ignored, or VDR logs it as an error
    } else {
        return false;
    }
    return true;
}

// --- cMenuSetupDVD --------------------------------------------------------

cMenuSetupDVD::cMenuSetupDVD() : data(DVDSetup) {
    SetSection(tr("Plugin.DVD$DVD"));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred menu language"), &data.MenuLanguage, ISO639codes, ISO639code));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred audio language"), &data.AudioLanguage, ISO639codes, ISO639code));
    Add(new cMenuEditStraItem(tr("Setup.DVD$Preferred subtitle language"), &data.SpuLanguage, ISO639codes, ISO639code));
    Add(new cMenuEditIntItem(tr("Setup.DVD$Player region code"), &data.PlayerRCE, 0, 8));
    Add(new cMenuEditBoolItem(tr("Setup.DVD$Display subtitles"), &data.ShowSubtitles));
    Add(new cMenuEditBoolItem(tr("Setup.DVD$Hide Mainmenu Entry"), &data.HideMainMenu));
    Add(new cMenuEditBoolItem(tr("Setup.DVD$ReadAHead"), &data.ReadAHead));
}

void cMenuSetupDVD::Store() {
    DVDSetup = data;
    SetupStore("MenuLanguage", DVDSetup.MenuLanguage);
    SetupStore("AudioLanguage", DVDSetup.AudioLanguage);
    SetupStore("SpuLanguage", DVDSetup.SpuLanguage);
    SetupStore("PlayerRCE", DVDSetup.PlayerRCE);
    SetupStore("ShowSubtitles", DVDSetup.ShowSubtitles);
    SetupStore("HideMainMenu", DVDSetup.HideMainMenu);
    SetupStore("ReadAHead", DVDSetup.ReadAHead);
}
