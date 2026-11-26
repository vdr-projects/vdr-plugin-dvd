/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#include "dvd.h"
#include "control-dvd.h"
#include "dvddev.h"
#include "setup-dvd.h"

// C++ Standard Library
#include <cstddef>
#include <cstdio>

// POSIX
#include <getopt.h>

// VDR
#include <vdr/config.h>
#include <vdr/i18n.h>
#include <vdr/menuitems.h>
#include <vdr/osdbase.h>
#include <vdr/player.h>
#include <vdr/plugin.h>
#include <vdr/tools.h>

#if APIVERSNUM < 20606
#error "VDR 2.6.6+ required (APIVERSNUM >= 20606)"
#endif

static const char *VERSION = "1.0.0 RC3";
static const char *DESCRIPTION = trNOOP("Plugin.DVD$turn VDR into an (almost) full featured DVD player");
static const char *MAINMENUENTRY = trNOOP("Plugin.DVD$DVD");

// --- cPluginDvd ------------------------------------------------------------

auto cPluginDvd::Description() -> const char * { return tr(DESCRIPTION); }

auto cPluginDvd::MainMenuEntry() -> const char * { return DVDSetup.HideMainMenu ? nullptr : tr(MAINMENUENTRY); }

auto cPluginDvd::Version() -> const char * { return VERSION; }

auto cPluginDvd::CommandLineHelp() -> const char * {
    return "  -C DEV,   --dvd=DEV      use DEV as the DVD device (default: /dev/cdrom)\n";
}

auto cPluginDvd::ProcessArgs(int argc, char *argv[]) -> bool {
    static struct option long_options[] = {
        {"dvd", required_argument, nullptr, 'C'},
        {nullptr, 0, nullptr, 0},
    };

    int c = 0;
    while ((c = getopt_long(argc, argv, "C:", long_options, nullptr)) != -1) {
        switch (c) {
            case 'C':
                cDVD::SetDeviceName(optarg);
                // A USB drive may be plugged in later, and returning false
                // here would keep VDR from starting at all.
                if (!cDVD::DriveExists()) {
                    isyslog("dvd: DVD drive %s not found (yet)", optarg);
                }
                break;
            default:
                fprintf(stderr, "arg char: %c\n", c);
                return false;
        }
    }
    return true;
}

auto cPluginDvd::MainMenuAction() -> cOsdMenu * {
    // VDR owns the control, which the analyzer can't see.
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    if (!cDvdPlayerControl::DVDActive()) {
        cControl::Launch(new cDvdPlayerControl);
    }
    return nullptr;
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

auto cPluginDvd::SetupMenu() -> cMenuSetupPage * { return new cMenuSetupDVD; }

auto cPluginDvd::SetupParse(const char *Name, const char *Value) -> bool { return DVDSetup.SetupParse(Name, Value); }

VDRPLUGINCREATOR(cPluginDvd); // Don't touch this!
