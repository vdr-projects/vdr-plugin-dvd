/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#include <unistd.h>
#include <string>

#include "dvddev.h"

// --- cDVD ------------------------------------------------------------------

// udev only creates /dev/cdrom (for sr0), no /dev/dvd
const char *cDVD::deviceName = "/dev/cdrom";

void cDVD::SetDeviceName(const char *DeviceName)
{
  // static, not new: the plugin keeps it until it is unloaded, and
  // LeakSanitizer then reports an object nobody deleted
  static std::string name;
  name = DeviceName;
  deviceName = name.c_str();
}

const char *cDVD::DeviceName(void)
{
  return deviceName;
}

bool cDVD::DriveExists(void)
{
  return access(deviceName, F_OK) == 0;
}
