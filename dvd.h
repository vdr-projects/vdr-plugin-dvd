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
  virtual const char *Version(void);
  virtual const char *Description(void);
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual const char *MainMenuEntry(void);
  virtual cOsdMenu *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
};
