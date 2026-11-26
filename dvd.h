/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#ifndef VDR_DVD_DVD_H
#define VDR_DVD_DVD_H

// VDR
#include <vdr/menuitems.h>
#include <vdr/osdbase.h>
#include <vdr/plugin.h>

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

#endif // VDR_DVD_DVD_H
