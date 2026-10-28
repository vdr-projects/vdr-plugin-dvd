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

#if APIVERSNUM < 20606
#error "VDR 2.6.6+ required (APIVERSNUM >= 20606)"
#endif

static const char *VERSION        = "0.4.0 devel";
static const char *DESCRIPTION    = trNOOP("Plugin.DVD$turn VDR into an (almost) full featured DVD player");
static const char *MAINMENUENTRY  = trNOOP("Plugin.DVD$DVD");

// --- cPluginDvd ------------------------------------------------------------

const char *cPluginDvd::Description(void) {
    return tr(DESCRIPTION);
}

const char *cPluginDvd::MainMenuEntry(void) {
    return DVDSetup.HideMainMenu ? NULL : tr(MAINMENUENTRY);
}

const char *cPluginDvd::Version(void) {
    return VERSION;
}

const char *cPluginDvd::CommandLineHelp(void)
{
    return   "  -C DEV,   --dvd=DEV      use DEV as the DVD device (default: /dev/cdrom)\n";
}

bool cPluginDvd::ProcessArgs(int argc, char *argv[])
{
    static struct option long_options[] = {
        { "dvd",      required_argument, NULL, 'C' },
        { NULL, 0, NULL, 0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, "C:", long_options, NULL)) != -1) {
        switch (c) {
            case 'C':
                cDVD::SetDeviceName(optarg);
                // A USB drive may be plugged in later, and returning false
                // here would keep VDR from starting at all.
                if (!cDVD::DriveExists())
                    isyslog("dvd: DVD drive %s not found (yet)", optarg);
                break;
            default:
                fprintf(stderr, "arg char: %c\n", c);
                return false;
        }
    }
    return true;
}

cOsdMenu *cPluginDvd::MainMenuAction(void)
{
    if (!cDvdPlayerControl::DVDActive())
        cControl::Launch(new cDvdPlayerControl);
    return NULL;
}

cMenuSetupPage *cPluginDvd::SetupMenu(void)
{
    return new cMenuSetupDVD;
}

bool cPluginDvd::SetupParse(const char *Name, const char *Value)
{
    return DVDSetup.SetupParse(Name, Value);
}

VDRPLUGINCREATOR(cPluginDvd); // Don't touch this!
