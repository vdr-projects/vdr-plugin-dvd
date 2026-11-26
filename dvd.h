/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#include <vdr/plugin.h>
#include <vdr/i18n.h>
#include <getopt.h>
#include "dvddev.h"
#include "tools-dvd.h"
#include "player-dvd.h"
#include "control-dvd.h"
#include "setup-dvd.h"

// --- cPluginDvd ------------------------------------------------------------

class cPluginDvd : public cPlugin {
public:
    const char *Version(void) override;
    const char *Description(void) override;
    const char *CommandLineHelp(void) override;
    bool ProcessArgs(int argc, char *argv[]) override;
    const char *MainMenuEntry(void) override;
    cOsdMenu *MainMenuAction(void) override;
    cMenuSetupPage *SetupMenu(void) override;
    bool SetupParse(const char *Name, const char *Value) override;
};
