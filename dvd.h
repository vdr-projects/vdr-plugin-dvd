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
    auto Version() -> const char * override;
    auto Description() -> const char * override;
    auto CommandLineHelp() -> const char * override;
    auto ProcessArgs(int argc, char *argv[]) -> bool override;
    auto MainMenuEntry() -> const char * override;
    auto MainMenuAction() -> cOsdMenu * override;
    auto SetupMenu() -> cMenuSetupPage * override;
    auto SetupParse(const char *Name, const char *Value) -> bool override;
};

#endif // VDR_DVD_DVD_H
