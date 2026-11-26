/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#ifndef VDR_DVD_DVDDEV_H
#define VDR_DVD_DVDDEV_H

class cDVD {
  private:
    static const char *deviceName;

  public:
    static void SetDeviceName(const char *Name);
    static const char *DeviceName(void);
    static bool DriveExists(void);
};

#endif // VDR_DVD_DVDDEV_H
