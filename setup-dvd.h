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

// C++ Standard Library
#include <array>
#include <cstddef>

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

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) -- VDR plugin model, like VDR's Setup
extern cDVDSetup DVDSetup;

// The DVD stores a two-letter ISO 639-1 language code with each stream. These
// are the codes offered as the preferred languages and passed to libdvdnav,
// plus the language names shown in VDR's menus (ASCII). VDR's own language
// handling uses three-letter codes and does not cover this.
inline constexpr std::array ISO639code{
    "en", "de", "sl", "it", "nl", "pt", "fr", "no", "fi", "pl", "es",
    "el", "sv", "ro", "hu", "ca", "ru", "hr", "et", "da", "cs", "tr",
};
inline constexpr int ISO639codes = static_cast<int>(ISO639code.size());

inline constexpr std::array DvdLanguageName{
    "English", "Deutsch",  "Slovenski", "Italiano", "Nederlands", "Portugues", "Francais", "Norsk",
    "Suomi",   "Polski",   "Espanol",   "Greek",    "Svenska",    "Romaneste", "Magyar",   "Catala",
    "Russian", "Hrvatski", "Eesti",     "Dansk",    "Czech",      "Turkce",
};
static_assert(DvdLanguageName.size() == ISO639code.size(), "a name for each code");

// The code of a language setup value, which SetupParse() keeps in range.
[[nodiscard]] inline auto LanguageCode(int Index) -> const char * { return ISO639code.at(static_cast<size_t>(Index)); }

#endif // VDR_DVD_SETUP_DVD_H
