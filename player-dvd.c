/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

// #define IFRAMEWRITE

#ifndef DEBUG
	#ifndef NDEBUG
		#define NDEBUG
	#endif
#endif

#include <sys/time.h>

#include <vdr/i18n.h>
#include <vdr/thread.h>
#include <vdr/device.h>
#include <vdr/plugin.h>

#include "common-dvd.h"
#include "i18n.h"
#include "dvddev.h"
#include "setup-dvd.h"
#include "tools-dvd.h"
#include "player-dvd.h"
#include "control-dvd.h"
#include "dvd.h"

/**
 * this was "weak"'s solution of a forced 
 * SPU only stream choice,
 * which does _not_ work ..
 *
 * Problem:
 *   - forcedDisplay | menu ctrseq within the SPU,
 *     means kinda "just do it"
 *   - special SPU channels, which only shows up
 *     e.g. if a klingon speaks klingon,
 *     are handled by a special SPU channel id,
 *     which dvdnav will choose for you automatic,
 *     and if your appropriate language is set
 *     as the default language !
#define FORCED_SUBS_ONLY_SEMANTICS
 *
 *     - we handle the forcedDisplay (outta a menu space)
 *     - we take care now of all SPU channels [0x00 .. 0x1F]
 *     - ...
 */

// #define NO_USLEEP
#ifdef NO_USLEEP
	#define USLEEP(a)
#else
	#define USLEEP(a)	usleep((a))
#endif

#ifdef DEBUG

#warning using verbose DEBUG mode

#define DVDDEBUG
#undef  DEBUGDVD
#define DEBUGDVD(format, args...) printf (format, ## args)
#define CTRLDEBUG
#undef DEBUG_CONTROL
#define DEBUG_CONTROL(format, args...) printf (format, ## args); fflush(NULL)
#define NAVDEBUG
#undef DEBUG_NAV
#define DEBUG_NAV(format, args...) printf (format, ## args); fflush(NULL)
/**
 **
 **
#define NAV0DEBUG
#undef  DEBUG_NAV0
#define DEBUG_NAV0(format, args...) printf (format, ## args); fflush(NULL)
 **
 **
 **
 **
#define SUBPDECDEBUG
#undef DEBUG_SUBP_DEC
#define DEBUG_SUBP_DEC(format, args...) printf (format, ## args); fflush(NULL)
 **
#define SUBPDEBUG
#undef DEBUG_SUBP_ID
#define DEBUG_SUBP_ID(format, args...) printf (format, ## args); fflush(NULL)
 */
#define IFRAMEDEBUG
#undef DEBUG_IFRAME
#define DEBUG_IFRAME(format, args...) printf (format, ## args); fflush(NULL)
#define PTSDEBUG
#undef DEBUG_PTS
#define DEBUG_PTS(format, args...) printf (format, ## args); fflush(NULL)
#define AUDIOIDDEBUG
#undef DEBUG_AUDIO_ID
#define DEBUG_AUDIO_ID(format, args...) printf (format, ## args); fflush(NULL)
#define AUDIOPLAYDEBUG
#undef DEBUG_AUDIO_PLAY
#define DEBUG_AUDIO_PLAY(format, args...) printf (format, ## args); fflush(NULL)

/**
#define AUDIOPLAYDEBUG2
#undef DEBUG_AUDIO_PLAY2
#define DEBUG_AUDIO_PLAY2(format, args...) printf (format, ## args); fflush(NULL)
 */


#endif


// --- static stuff ------------------------------------------------------
#if 1
#ifdef POLLTIMEOUTS_BEFORE_DEVICECLEAR
	// polltimeout of 3 seems to be enough for a softdevice ..
	#undef POLLTIMEOUTS_BEFORE_DEVICECLEAR 
#endif
#endif

#ifndef POLLTIMEOUTS_BEFORE_DEVICECLEAR
#define POLLTIMEOUTS_BEFORE_DEVICECLEAR 3
#else
#warning using patched POLLTIMEOUTS_BEFORE_DEVICECLEAR
#endif

#if defined( HAVE_AC3_OVER_DVB )
#define playMULTICHANNEL         Setup.PlayMultichannelAudio
#else
#define playMULTICHANNEL         false
#endif

#define OsdInUse() ((controller!=NULL)?controller->OsdVisible(this):false)
#define TakeOsd()  ((controller!=NULL)?controller->TakeOsd((void *)this):false)

#ifdef CTRLDEBUG

static void printCellInfo( dvdnav_cell_change_event_t & lastCellEventInfo, 
			   int64_t pgcTotalTicks, int64_t pgcTicksPerBlock )
{
    int cell_length_s = (int)(lastCellEventInfo.cell_length / 90000L) ;
    int pg_length_s = (int)(lastCellEventInfo.pg_length / 90000L) ;
    int pgc_length_s = (int)(lastCellEventInfo.pgc_length / 90000L) ;
    int cell_start_s = (int)(lastCellEventInfo.cell_start / 90000L) ;
    int pg_start_s = (int)(lastCellEventInfo.pg_start / 90000L) ;

    printf("cellN %ld, pgN=%d, cell_length=%ld %d, pg_length=%ld %d, pgc_length=%ld %d, cell_start %ld %d, pg_start %ld %d, ticksPerBlock=%ld, secPerBlock=%ld\n",
    		(long)lastCellEventInfo.cellN ,
    		lastCellEventInfo.pgN ,
    		(long)lastCellEventInfo.cell_length ,
		cell_length_s,
    		(long)lastCellEventInfo.pg_length ,
		pg_length_s,
    		(long)lastCellEventInfo.pgc_length ,
		pgc_length_s,
    		(long)lastCellEventInfo.cell_start ,
		cell_start_s,
    		(long)lastCellEventInfo.pg_start ,
		pg_start_s,
		(long)pgcTicksPerBlock, (long)(pgcTicksPerBlock/90000L)
	      );
}

#endif

const char *cDvdPlayer::PTSTicksToHMSm(int64_t ticks, bool WithMS)
{
  static char buffer[200];

  int ms = (int)(ticks / 90L) % 100;

  int s = (int)(ticks / 90000L);
  int m = s / 60 % 60;
  int h = s / 3600;
  s %= 60;
  snprintf(buffer, sizeof(buffer), WithMS ? "%d:%02d:%02d.%02d" : "%d:%02d:%02d", h, m, s, ms);
  return buffer;
}


// --- cDvdPlayer ------------------------------------------------------------

//XXX+ also used in recorder.c - find a better place???
// The size of the array used to buffer video data:
// (must be larger than MINVIDEODATA - see remux.h)
#define VIDEOBUFSIZE  MEGABYTE(1)

// The number of frames to back up when resuming an interrupted replay session:
#define RESUMEBACKUP (10 * FRAMESPERSEC)

#define MAX_VIDEO_SLOWMOTION 63 // max. arg to pass to VIDEO_SLOWMOTION // TODO is this value correct?
#define NORMAL_SPEED 10 // the index of the '1' entry in the following array
#define MAX_SPEEDS    3 // the offset of the maximum speed from normal speed in either direction,
		        // for the dvbplayer
#define MAX_MAX_SPEEDS MAX_SPEEDS*MAX_SPEEDS // the super speed maximum			
#define SPEED_MULT   12 // the speed multiplier
int cDvdPlayer::Speeds[] = { 0, 0, 0, 0, 0, 0, 0, -2, -4, -8, 1, 2, 4, 12, 0, 0, 0, 0, 0, 0, 0 };
bool cDvdPlayer::BitStreamOutActive = false;
bool cDvdPlayer::HasBitStreamOut = false;
bool cDvdPlayer::HasSoftDeviceOut = false;
bool cDvdPlayer::SoftDeviceOutActive = false;

const int cDvdPlayer::MaxAudioTracks  = 0x20;
const int cDvdPlayer::AudioTrackMask  = 0x07;
const int cDvdPlayer::MaxSubpStreams  = 0x20;
const int cDvdPlayer::SubpStreamMask  = 0x1F;

const char * cDvdPlayer::dummy_title     = "DVD Title";
const char * cDvdPlayer::dummy_n_a       = "n.a.";

#if VDRVERSNUM<=10206
cDvdPlayer::cDvdPlayer(void): a52dec(*this) {
#else
cDvdPlayer::cDvdPlayer(void): cThread("dvd-plugin"), a52dec(*this) {
#endif
    DEBUGDVD("cDvdPlayer::cDvdPlayer(void)\n");
    ringBuffer=new cRingBufferFrame(VIDEOBUFSIZE);
    rframe=NULL;
    pframe=NULL;
    controller = NULL;
    active = true;
    running = false;
    playMode = pmPlay;
    playDir = pdForward;
    trickSpeed = NORMAL_SPEED;
    stillTimer = 0;
    currButtonN = -1;
    nav = 0;
    iframeAssembler=new cRingBufferLinear(KILOBYTE(CIF_MAXSIZE));
    IframeCnt = 0;
    current_pci = 0;
    prev_e_ptm = 0;
    ptm_offs = 0;
    DVDSetup.ShowSubtitles == 2 ? forcedSubsOnly = true : forcedSubsOnly = false;
    DEBUG_SUBP_ID("SPU showSubs=%d, forcedSubsOnly=%d\n", DVDSetup.ShowSubtitles, forcedSubsOnly);

    skipPlayVideo=0;
    fastWindFactor=1;

    clearSeenSubpStream();
    clearSeenAudioTrack();

    DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: curNavAu set to 0x00\n");

    currentNavAudioTrackType = -1;
    lastNavAudioTrackType = -1;

    SPUdecoder = NULL;
    a52dec.setSyncMode(A52decoder::ptsGenerate);

    titleinfo_str=0;
    title_str=0;
    aspect_str=0;
    SetTitleInfoString();
    SetTitleString();
    SetAspectString();
}

cDvdPlayer::~cDvdPlayer()
{
  DEBUGDVD("destructor cDvdPlayer::~cDvdPlayer()\n");
  Empty();
  Detach();
  Save();
  delete iframeAssembler;
  delete ringBuffer;
  ringBuffer=NULL;
  if(titleinfo_str) { free(titleinfo_str); titleinfo_str=0; }
  if(title_str) { free(title_str); title_str=0; }
  if(aspect_str) { free(aspect_str); aspect_str=0; }
}

void cDvdPlayer::setController (cDvdPlayerControl *ctrl )
{
    controller = ctrl;
}

bool cDvdPlayer::IsDvdNavigationForced() const
{
    return (controller?controller->IsDvdNavigationForced():false);
}

void cDvdPlayer::TrickSpeed(int Increment)
{
  int nts = trickSpeed + Increment;
  cntBlocksPlayed=0;

  if ( nts>0 && nts-NORMAL_SPEED <= MAX_SPEEDS && Speeds[nts] == 1) {
     fastWindFactor  = 1;
     trickSpeed = nts;

     if (playMode == pmFast)
        Play();
     else
        Pause();
  } else 
  if ( nts>0 && nts-NORMAL_SPEED <= MAX_SPEEDS && Speeds[nts]) {
     fastWindFactor  = 1;
     if ( playDir == pdBackward && DVDSetup.ReadAHead>0 )
	     dvdnav_set_readahead_flag(nav, 0);
     trickSpeed = nts;
     int Mult = ( playMode == pmSlow ) ? 1 : SPEED_MULT;
     int sp = (Speeds[nts] > 0) ? Mult / Speeds[nts] : -Speeds[nts] * Mult;

     if (sp > MAX_VIDEO_SLOWMOTION)
        sp = MAX_VIDEO_SLOWMOTION;

     if ( playDir == pdBackward )
     {
	     fastWindFactor  = ( playMode == pmSlow ) ? trickSpeed - NORMAL_SPEED     :
	     					        trickSpeed - NORMAL_SPEED + 1 ;
	     if ( playMode == pmSlow ) sp=2;
     }
     DeviceTrickSpeed(sp);

  } else 
  if ( nts>0 && nts-NORMAL_SPEED <= MAX_MAX_SPEEDS ) {
     fastWindFactor  = 1;
     trickSpeed = nts;
     if ( playDir == pdBackward )
     	  if ( playMode == pmSlow )
	          fastWindFactor  = trickSpeed - NORMAL_SPEED ;
	  else
	  	  fastWindFactor  = ( trickSpeed - NORMAL_SPEED - MAX_SPEEDS + 1 ) * 2 ;
     else if ( playDir == pdForward && playMode == pmFast )
	  fastWindFactor  = ( trickSpeed - NORMAL_SPEED - MAX_SPEEDS + 1 ) * 2 ;

  }

  DEBUG_CONTROL("dvd TRICKSPEED: %d (%d), fastWindFactor=%d fwd=%d, slow=%d, fast=%d\n", 
     	trickSpeed, Increment, fastWindFactor, playDir == pdForward, playMode == pmSlow, playMode == pmFast);

}

void cDvdPlayer::DeviceClear(void)
{
  DEBUG_CONTROL("DeviceClear\n");
  cPlayer::DeviceClear();
}

void cDvdPlayer::DevicePlay(void)
{
  DEBUG_CONTROL("DevicePlay\n");
  cPlayer::DevicePlay();
}

void cDvdPlayer::DrawSPU()
{
  if (SPUdecoder && !SPUdecoder->IsVisible() && TakeOsd()) {
	  SPUdecoder->Draw();
  }
}

void cDvdPlayer::HideSPU()
{
  if (SPUdecoder) {
	  SPUdecoder->Hide();
  }
}

void cDvdPlayer::EmptySPU()
{
  if (SPUdecoder) {
	  ClearButtonHighlight();
	  SPUdecoder->Empty();
  }
}

void cDvdPlayer::Empty(bool emptyDeviceToo)
{
  DEBUG_CONTROL("dvd .. Empty ...\n");

  LOCK_THREAD;

  if( IframeCnt < 0 ) {
            DEBUG_IFRAME("I-Frame: Empty: Iframe used -> emptyDevice\n");
	    emptyDeviceToo=true;
  }

  DEBUG_CONTROL("dvd .. Empty IframeCnt=%d, emptyDeviceToo=%d!\n", IframeCnt, emptyDeviceToo);

  if (SPUdecoder) {
	  DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
          EmptySPU();
  }
  currButtonN = -1;
  a52dec.clear();
  lastFrameType     = 0xff;
  VideoPts          = 0xFFFFFFFF;
  stcPTS            = 0xFFFFFFFF;
  stcPTSLast        = 0xFFFFFFFF;
  stcPTSAudio       = 0xFFFFFFFF;
  stcPTSLastAudio   = 0xFFFFFFFF;
  pktpts            = 0xFFFFFFFF;
  pktptsLast        = 0xFFFFFFFF;
  pktptsAudio       = 0xFFFFFFFF;
  pktptsLastAudio   = 0xFFFFFFFF;
  prev_e_ptm = 0;
  ptm_offs = 0;

  delete rframe;
  rframe=NULL; pframe=NULL;
  ringBuffer->Clear();
  cntBlocksPlayed = 0;

  stillTimer = 0;
  stillFrame = 0;
  IframeCnt = 0;
  iframeAssembler->Clear();

  skipPlayVideo=0;

  if(emptyDeviceToo) {
	  DeviceClear();
  }
}

void cDvdPlayer::StillSkip()
{
      LOCK_THREAD;
      DEBUGDVD("cDvdPlayer::StillSkip()\n");
      if(stillTimer!=0)
      {
          stillTimer = 0;
          if(nav!=NULL)
          {
	      dvdnav_still_skip(nav);
          }
      }
}

#ifdef DVDDEBUG
static char *dvd_ev[] =
    { "BLOCK_OK",
      "NOP",
      "STILL_FRAME",
      "SPU_STREAM_CHANGE",
      "AUDIO_STREAM_CHANGE",
      "VTS_CHANGE",
      "CELL_CHANGE",
      "NAV_PACKET",
      "STOP",
      "HIGHLIGHT",
      "SPU_CLUT_CHANGE",
      "SEEK_DONE",                   // remove in newer lib versions
      "HOP_CHANNEL",
      "WAIT",                        // since 0.1.7
      "TRICK_MODE_CHANGE" };
#endif

#define CELL_STILL       0x02
#define NAV_STILL        0x04

/**
  * timer funktions with ticks as its base
  *
  * 90000 ticks are 1 second, acording to MPEG !
  * 
  * just to make things clear ;-)
  *
  * 1000 ms / s, 1000000 us / s 
  *
  * 90000/1 t/s = 90/1 t/ms = 9/100 t/us
  *
  * 1/90000 s/t = 1/90 ms/t = 100/9 us/t
  */
uint64_t cDvdPlayer::time_ticks(void)
{
  static time_t t0 = 0;
  struct timeval t;
  if (gettimeofday(&t, NULL) == 0) {
     if (t0 == 0)
        t0 = t.tv_sec; // this avoids an overflow (we only work with deltas)
     return (uint64_t)(t.tv_sec - t0) * 90000U + 
            (uint64_t)( t.tv_usec * 9U ) / 100U ;
     }
  return 0;
}

uint64_t cDvdPlayer::delay_ticks(uint64_t ticks)
{
#ifndef NO_USLEEP
  uint64_t t0 = time_ticks();
  USLEEP(1000); // at least one guaranteed sleep of 1ms !!
  uint64_t done = time_ticks() - t0 ;
  while ( ticks > done )
  {
	// USLEEP resol.  is about 19 ms
	if(ticks-done>19U*90U) 
		USLEEP( (ticks-done)*100U/9U - 19000U );
        done = time_ticks() - t0 ;
  }		
  return time_ticks() - t0 ;
#else
  return ticks;
#endif
}

void cDvdPlayer::Action(void) {

#if VDRVERSNUM<=10206
    dsyslog("input thread started (pid=%d)", getpid());
#endif

  unsigned char  event_buf[2048];
  memset(event_buf, 0, sizeof(event_buf));

  unsigned char *write_blk = NULL;
  int blk_size = 0;

  BitStreamOutActive   	= false;
  HasBitStreamOut      	= (cPluginManager::GetPlugin("bitstreamout") != NULL);

  SoftDeviceOutActive 	= false;
  HasSoftDeviceOut 	= (cPluginManager::GetPlugin("xine") != NULL);

  cSetupLine *slBitStreamOutActive = NULL;
  if(HasBitStreamOut) {
	slBitStreamOutActive = cPluginDvd::GetSetupLine("active", "bitstreamout");
	if(slBitStreamOutActive!=NULL)
		BitStreamOutActive = atoi ( slBitStreamOutActive->Value() ) ? true: false ;
  }
  printf("dvd player: BitStreamOutActive=%d, HasBitStreamOut=%d (%d)\n", 
  	BitStreamOutActive, HasBitStreamOut, slBitStreamOutActive!=NULL);

  if(HasSoftDeviceOut) {
  	SoftDeviceOutActive 	= true;
  }
  printf("dvd player: SoftDeviceOutActive=%d, HasSoftDeviceOut=%d\n", SoftDeviceOutActive, HasSoftDeviceOut);

  if (dvdnav_open(&nav, const_cast<char *>(cDVD::getDVD()->DeviceName())) != DVDNAV_STATUS_OK) {
      dsyslog("input thread ended (pid=%d)", getpid());
      MSG_ERROR(tr("Error.DVD$Error opening DVD!"));
      printf("dvd player: cannot open dvdnav device %s -> input thread ended (pid=%d) !\n", 
      	const_cast<char *>(cDVD::getDVD()->DeviceName()), getpid());
      active = running = false;
      nav=NULL;
      fflush(NULL);
      return;
  }
  dvdnav_set_readahead_flag(nav, DVDSetup.ReadAHead);
  if (DVDSetup.PlayerRCE != 0)
      dvdnav_set_region_mask(nav, 1 << (DVDSetup.PlayerRCE - 1));
  else
      dvdnav_set_region_mask(nav, 0xffff);
  dvdnav_menu_language_select(nav,  const_cast<char *>(ISO639code[DVDSetup.MenuLanguage]));
  dvdnav_audio_language_select(nav, const_cast<char *>(ISO639code[DVDSetup.AudioLanguage]));
  dvdnav_spu_language_select(nav,   const_cast<char *>(ISO639code[DVDSetup.SpuLanguage]));
  DEBUGDVD("Default-Langs: menu=%s, audio=%s, spu=%s\n", 
	  const_cast<char *>(ISO639code[DVDSetup.MenuLanguage]),
	  const_cast<char *>(ISO639code[DVDSetup.AudioLanguage]),
	  const_cast<char *>(ISO639code[DVDSetup.SpuLanguage]));
  if (IsAttached())
  {
      SPUdecoder = cDevice::PrimaryDevice()->GetSpuDecoder();
      DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
      EmptySPU();
  }

  int slomoloop=0;
  uint64_t sleept = 0; // in ticks !
  uint64_t sleept_done = 0; // in ticks !
  bool trickMode = false;
  bool noAudio   = false;
  int PollTimeouts = 0;
  bool isInMenuDomain = false;
  int playedPacket = pktNone;

  uint32_t cntBlocksSkipped  = 0;
  cntBlocksPlayed = 0;

  Empty();
  IframeCnt = -1; // mark that we have to reset the device, before 1st PlayVideo ..

  running = true;
  eFrameType frameType=ftUnknown;
  while( running && nav ) 
  {
      isInMenuDomain = IsInMenuDomain();

      if ( !pframe ) {
            pframe=ringBuffer->Get();
            if ( pframe ) {
                write_blk=pframe->Data();
                blk_size=pframe->Count();
                frameType=pframe->Type();
            }
      }

      // clip PTS values ..
      if ( pktptsAudio<pktptsLastAudio )
	pktptsLastAudio=pktptsAudio;

      if ( stcPTSAudio<stcPTSLastAudio )
        stcPTSLastAudio=stcPTSAudio;

      if ( pktpts<pktptsLast )
	pktptsLast=pktpts;

      if ( stcPTS<stcPTSLast )
        stcPTSLast=stcPTS;

      if ( playedPacket==pktAudio )
      {
          uint64_t sleept_trial = 0; // in ticks !

 	  // do extra sleep, if stream's pts of audio diffs
	  // against dvb device's seen pts more/equal than 1 ms == 90 ticks !
          if ( pktptsAudio-pktptsLastAudio >= stcPTSAudio-stcPTSLastAudio+90 ) 
	  {
	      sleept_trial = (pktptsAudio-pktptsLastAudio) - 
	                     (stcPTSAudio-stcPTSLastAudio) ;
	      if(sleept_trial>sleept) sleept=sleept_trial;
	  }
      }

      sleept_done = 0;
      if (sleept) 
      {
	  if ( sleept/90U > 1000 ) 
		DEBUG_PTS("\n***** WARNING >=1000ms sleep %llums\n", sleept/90U);
          sleept_done = delay_ticks(sleept);

          DEBUG_PTS2("dvd loop sleep=%5ut(%3ums)/%5ut(%3ums), blk_size=%3d, skipPlayV=%d, AudioBlock=%d IframeCnt=%d stillTimer=%u\n", 
		(unsigned int)sleept, (unsigned int)sleept/90U, 
		(unsigned int)sleept_done, (unsigned int)sleept_done/90U, 
		blk_size, skipPlayVideo,
                frameType==ftAudio,
		IframeCnt, stillTimer/90U);
      }
      sleept = 0;

/**
        DEBUG_PTS("dvd loop stc=%8ums (d%3ums,d%3ums) pts=%8ums (%3ums,d%3ums)\n",
	(unsigned int)(stcPTS/90U), 
	(unsigned int)((stcPTS-stcPTSLast)/90U), 
	(unsigned int)((stcPTSAudio-stcPTSLastAudio)/90U), 
	(unsigned int)(pktpts/90U), 
	(unsigned int)((pktpts-pktptsLast)/90U), 
	(unsigned int)((pktptsAudio-pktptsLastAudio)/90U));
 */

      if (playMode == pmPause || playMode == pmStill) {
	  sleept = 10*90U;  // 10ms*90t/ms
	  continue;
      }

      cPoller Poller;
      if ( ! DevicePoll(Poller, 100) )
      {
        PollTimeouts++;
        if (PollTimeouts == POLLTIMEOUTS_BEFORE_DEVICECLEAR) 
        {
	  dsyslog("clearing device because of consecutive poll timeouts %d", 
		POLLTIMEOUTS_BEFORE_DEVICECLEAR);
          DEBUG_CONTROL("clearing device because of consecutive poll timeouts %d", 
		POLLTIMEOUTS_BEFORE_DEVICECLEAR);
	  DeviceClear();
	  DevicePlay();
          PollTimeouts = 0;
	}
      	continue;
      }
      PollTimeouts = 0;

      LOCK_THREAD;

      trickMode = playMode == pmFast || (playMode == pmSlow && playDir == pdBackward) ;

      if ( pframe ) 
      {
          int res = blk_size;
          if( !skipPlayVideo ) 
	  {
                if ( IframeCnt < 0 )
		{
			// we played an IFrame with DeviceStillPicture, or else -> reset !
                        DEBUG_CONTROL("clearing device because of IframeCnt < 0\n");
			IframeCnt = 0;
			DeviceClear();
			DevicePlay();
		}
                res = PlayVideo(write_blk, blk_size); 

		if (trickMode) {
		      DEBUG_CONTROL("PLAYED  : todo=%d, written=%d\n",
				blk_size, res);
		}
 	  } 
#ifdef CTRLDEBUG
          else if (trickMode) 
		      printf("SKIPPED : todo=%d\n", blk_size);
#endif

	  if (res < 0) 
	  {
	      if (errno != EAGAIN && errno != EINTR) 
	      {
		  esyslog("ERROR: PlayVideo, %s (workaround activ)\n", strerror(errno));
		  printf("ERROR: PlayVideo, %s (workaround activ)\n", strerror(errno));
	      }
	      DEBUG_CONTROL("PLAYED zero -> Clear/Play\n");
	      DeviceClear();
	      DevicePlay();
	      continue;
	  }
	  if (res > 0) {
	        blk_size -= res;
                write_blk += res;
	  }

	  if (blk_size > 0) {
	      sleept = 5*90U;  // 5ms*90t/ms
	  } else 
	  {
              playedPacket = pktNone;
              frameType=ftUnknown;
              ringBuffer->Drop(pframe);
              pframe=0;

	      if(!skipPlayVideo)
		cntBlocksPlayed++;
	      else
		cntBlocksSkipped++;
	  }
	  continue;
      } else {
            playedPacket = pktNone;
            frameType=ftUnknown;
      }
      if (IframeCnt > 0) 
      {
	    DEBUG_CONTROL("clearing device because of IframeCnt > 0\n");
            DeviceClear();
            DevicePlay();
            int iframeSize=iframeAssembler->Available();
            unsigned char *iframe=iframeAssembler->Get(iframeSize);

            DEBUG_IFRAME("I-Frame: DeviceStillPicture: IframeCnt=%d->-1, iframe=%d, used=%d; ", 
	    	IframeCnt, iframe!=NULL, iframeSize);

            if ( iframe && iframeSize>0 ) {
#ifdef IFRAMEWRITE
		static int ifnum = 0;
		static char ifname[255];
		snprintf(ifname, 255, "/tmp/dvd.iframe.%3.3d.asm.pes", ifnum++);
	        FILE *f = fopen(ifname, "wb");
	        fwrite(iframe, 1, iframeSize, f);
	        fclose(f);
#endif
	    	DeviceStillPicture(iframe, iframeSize);
                DEBUG_IFRAME("SEND !\n");
	    }
//          iframeAssembler->Clear(); // FIXME ??
	    IframeCnt = -1; // mark that we played an IFrame !
            if (blk_size <= 0 && !skipPlayVideo)
	      sleept = 1*90U;  // 1ms*90t/ms

            DEBUG_IFRAME("I-Frame: DeviceStillPicture: stc=%8ums vpts=%8ums sleept=%llums\n", 
		(unsigned int)(stcPTS/90U), 
		(unsigned int)(VideoPts/90U), sleept/90U);
	    continue;
      }

      /**
       * check is we should use the very fast forward mode
       */
      if (playDir == pdForward && playMode == pmFast && 
          skipPlayVideo && cntBlocksPlayed>0 && fastWindFactor>1)
      {
	    uint32_t pos, len;
            int64_t pgcPosTicks = 0, pgcPosTicksIncr=0;

	    /**
	     * skip blocks .. if pos is not at the end
	     */
	    dvdnav_set_PGC_positioning_flag ( nav, 1);
	    dvdnav_get_position ( nav, &pos, &len);

            pgcPosTicks = (int64_t)pos * pgcTicksPerBlock;

 	    /**
	     * increment about fastWindFactor * 0.5 s
	     */
            pgcPosTicksIncr = (int64_t) ((fastWindFactor * 90000L)/2L) ;

	    if( pgcPosTicks+pgcPosTicksIncr < pgcTotalTicks )
	    {
		      DEBUG_CONTROL("dvd %d %4.4u/%4.4u fwd get block: %4.4ldb %10.10ldt %lds\n", 
				playDir == pdBackward, 
				cntBlocksPlayed, cntBlocksSkipped, 
				(long)pos, (long)pgcPosTicks, (long)(pgcPosTicks/90000L));

		      DEBUG_CONTROL("dvd fwd set block: ts=%d, factor=%ds, %ldms+%ldms=%ldms, %4.4ldb+%4.4ldb=%4.4ldb, %10.10ldt+%10.10ldt=%10.10ldt\n", 
					trickSpeed, fastWindFactor/2,
					(long)(pgcPosTicks/90L), (long)(pgcPosTicksIncr/90L), 
					(long)((pgcPosTicks+pgcPosTicksIncr)/90L), 
					(long)pos, (long)(pgcPosTicksIncr/pgcTicksPerBlock), 
					(long)((pgcPosTicks+pgcPosTicksIncr)/pgcTicksPerBlock),
					(long)pgcPosTicks, (long)pgcPosTicksIncr, (long)pgcPosTicks+(long)pgcPosTicksIncr);

		      pos = (pgcPosTicks+pgcPosTicksIncr)/pgcTicksPerBlock;

		      if ( dvdnav_sector_search( nav, pos, SEEK_SET) != 
		           DVDNAV_STATUS_OK )
		      	printf("dvd error dvdnav_sector_search: %s\n", 
				dvdnav_err_to_string(nav));
	    }
	    cntBlocksPlayed=0;
	    cntBlocksSkipped=0;
      } 

      /**
       * check is we should use any backward mode
       */
      else if (playDir == pdBackward && skipPlayVideo && cntBlocksPlayed>0 && fastWindFactor!=1)
      {
	    uint32_t pos=0, posdiff=0, len=0;
            int64_t pgcPosTicks = 0;

            if(fastWindFactor<0)
            {
	    	    /** 
		     * special slomo handling 
		     */
		    if(slomoloop==0)
		    {
			slomoloop= abs(fastWindFactor);
		        cntBlocksPlayed++; // one more block back ..
			posdiff = (uint32_t)(cntBlocksPlayed*2) ;
			DEBUG_CONTROL("dvd slomo jump: d%ldb %dsl (%df)\n", 
				(long)posdiff, slomoloop, fastWindFactor);
		    } else {
			posdiff = (uint32_t)(cntBlocksPlayed) ;
			DEBUG_CONTROL("dvd slomo loop: d%ldb %dsl (%df)\n", 
				(long)posdiff, slomoloop, fastWindFactor);
		        slomoloop--;
		    }
            } else {
	    	    /** 
		     * simple rewind:
  		     * 		trickspeed-NORMAL_SPEED <= MAX_SPEEDS 
		     * else fast rewind
		     *		half a secound * fastWindFactor
		     */
		    if ( trickSpeed-NORMAL_SPEED <= MAX_SPEEDS )
		    {
			    cntBlocksPlayed++; // one more block back ..
			    posdiff = cntBlocksPlayed*fastWindFactor ;
		    } else {
			    int64_t pgcPosTicksDecr=0;

			    /**
			     * increment about fastWindFactor * 0.5 s
			     */
			    pgcPosTicksDecr = (int64_t) ((fastWindFactor * 90000L)/2L) ;
			    posdiff = (uint32_t) (pgcPosTicksDecr / pgcTicksPerBlock);
		    }
	    }

	    /**
	     * jump back, if pos is not at the beginning
	     */
	    dvdnav_set_PGC_positioning_flag ( nav, 1);
	    if( dvdnav_get_position ( nav, &pos, &len) == DVDNAV_STATUS_OK && 
		pos>posdiff )
	    {
            	      pgcPosTicks = (int64_t)pos * pgcTicksPerBlock;
  		      uint32_t forcedBlockPosition = pos-posdiff;
		      DEBUG_CONTROL("dvd %d %4.4u/%4.4u bwd get block: %4.4ldb %10.10ldt %lds\n", 
				playDir == pdBackward, 
				cntBlocksPlayed, cntBlocksSkipped, 
				(long)pos, (long)pgcPosTicks, (long)(pgcPosTicks/90000L));
		      DEBUG_CONTROL("dvd bwd set block: ts=%d, factor=%d, %ld-%ld=%ld, slo=%d\n", 
					trickSpeed, fastWindFactor,
					(long)pos, (long)posdiff, (long)forcedBlockPosition, 
					slomoloop);
		      if ( dvdnav_sector_search( nav, forcedBlockPosition, SEEK_SET) != 
		           DVDNAV_STATUS_OK )
		      	printf("dvd error dvdnav_sector_search: %s\n", 
				dvdnav_err_to_string(nav));
	    } else {
		      DEBUG_CONTROL("dvd %d %4.4u/%4.4u bwd get block: %4.4ldb d%4.4ldb, slo=%d\n",
				playDir == pdBackward, 
				cntBlocksPlayed, cntBlocksSkipped, 
				(long)pos, (long)posdiff, slomoloop);
	    }
	    cntBlocksPlayed=0;
	    cntBlocksSkipped=0;
      } 
      else if ( playMode == pmFast || playMode == pmSlow )
      {
	    uint32_t pos, len;
            int64_t pgcPosTicks = 0;

	    dvdnav_set_PGC_positioning_flag ( nav, 1);
	    dvdnav_get_position ( nav, &pos, &len);

            pgcPosTicks = (int64_t)pos * pgcTicksPerBlock;

	    DEBUG_CONTROL("dvd %d %4.4u/%4.4u any get block: %4.4ldb %10.10ldt %lds, sec=%d\n", 
			playDir == pdBackward, 
			cntBlocksPlayed, cntBlocksSkipped, 
			(long)pos, (long)pgcPosTicks, (long)(pgcPosTicks/90000L), 
			(int)(pgcPosTicks/90000L));

	    /**
	     * dont jump over the end .. 10s tolerance ..
	     */
            if (playDir == pdForward && pos+1 == len && pgcPosTicks>90000L*10L)
	    {
		      pgcPosTicks-=90000L*10L;

		      if ( dvdnav_sector_search( nav, pgcPosTicks/pgcTicksPerBlock, SEEK_SET) != 
		           DVDNAV_STATUS_OK )
		      	printf("dvd error dvdnav_sector_search: %s\n", 
				dvdnav_err_to_string(nav));
	    }
      }

      unsigned char *cache_ptr = event_buf;
      int event;
      int len;

      // from here on, continue is not allowed,
      // as it would bypass dvdnav_free_cache_block
      if (dvdnav_get_next_cache_block(nav, &cache_ptr, &event, &len) != DVDNAV_STATUS_OK) {
      MSG_ERROR(tr("Error.DVD$Error fetching data from DVD!"));
	  running = false;
	  break;
      }
      
      noAudio   = playMode != pmPlay ;

      switch (event) {
	  case DVDNAV_BLOCK_OK:
	      // DEBUG_NAV("%s:%d:NAV BLOCK OK\n", __FILE__, __LINE__);
              playedPacket = playPacket(cache_ptr, trickMode, noAudio);
	      break;
	  case DVDNAV_NOP:
	      DEBUG_NAV("%s:%d:NAV NOP\n", __FILE__, __LINE__);
	      break;
	  case DVDNAV_STILL_FRAME: {
	      uint64_t currentTicks = time_ticks();
	      DEBUG_PTS2("%s:%d:NAV STILL FRAME (menu=%d), rem. stillTimer=%ut(%ums), currTicks=%llut(%llums)\n", 
			__FILE__, __LINE__, isInMenuDomain, 
			stillTimer, stillTimer/90, currentTicks, currentTicks/90);
	      dvdnav_still_event_t *still = (dvdnav_still_event_t *)cache_ptr;
	      if (stillTimer != 0) {
	          seenVPTS(0);
		  if (stillTimer > currentTicks) {
		      sleept = 40*90U;  // 40ms*90t/ms
		  } else {
		      StillSkip();
		      sleept = 0;
		      DEBUG_PTS("Still time clear\n");
		  }
		  DEBUG_PTS2("StillTimer->Sleep: %llut(%llums)\n", sleept, sleept/90);
	      } else {
	      	  if(still->length == 0xff) {
		  	stillTimer = INT_MAX;
		        DEBUG_PTS("Still time (max): %ut(%ums)\n", stillTimer, stillTimer/90);
		  } else {
		  	stillTimer = currentTicks + (uint64_t)(still->length) * 90000U;
		        DEBUG_PTS("Still time (set): %ut(%ums)\n", stillTimer, stillTimer/90);
		  }
	          SendIframe( true );
	      }
	      break;
	  }

          case DVDNAV_WAIT:
	      DEBUG_NAV("%s:%d:NAV WAIT\n", __FILE__, __LINE__);
	      prev_e_ptm = 0;
	      ptm_offs = 0;
	      dvdnav_wait_skip(nav);
	      break;

	  case DVDNAV_SPU_STREAM_CHANGE: {
	      DEBUG_NAV("%s:%d:NAV SPU STREAM CHANGE\n", __FILE__, __LINE__);
	      dvdnav_spu_stream_change_event_t *ev = (dvdnav_spu_stream_change_event_t *)cache_ptr;
	      DEBUG_SUBP_ID("SPU Streams: w:0x%X (%d), l:0x%X (%d), p:0x%X (%d), L:0x%X (%d), locked=%d/%d\n",
		    ev->physical_wide, ev->physical_wide, 
		    ev->physical_letterbox, ev->physical_letterbox, 
		    ev->physical_pan_scan, ev->physical_pan_scan,
		    ev->logical, ev->logical,
	            currentNavSubpStreamUsrLocked, !changeNavSubpStreamOnceInSameCell);

	      if( isInMenuDomain || IsDvdNavigationForced() ||
	          ( (!currentNavSubpStreamUsrLocked||changeNavSubpStreamOnceInSameCell) && 
		     DVDSetup.ShowSubtitles 
		    ) 
		  )
	      {
		      cSpuDecoder::eScaleMode mode = getSPUScaleMode();

		      if (mode == cSpuDecoder::eSpuLetterBox ) {
		          // TV 4:3,  DVD 16:9
			  currentNavSubpStream = ev->physical_letterbox & SubpStreamMask;
			  DEBUG_SUBP_ID("dvd choosing letterbox SPU stream: curNavSpu=%d 0x%X\n",
			     currentNavSubpStream, currentNavSubpStream);
		      } else {
			  currentNavSubpStream = ev->physical_wide & SubpStreamMask;
			  DEBUG_SUBP_ID("dvd choosing wide SPU stream: curNavSpu=%d 0x%X\n",
			     currentNavSubpStream, currentNavSubpStream);
		      }
		      changeNavSubpStreamOnceInSameCell=false;
	      } else {
		      DEBUG_SUBP_ID("DVDNAV_SPU_STREAM_CHANGE: ignore (locked=%d/%d|not enabled=%d), menu=%d, feature=%d \n", 
		          currentNavSubpStreamUsrLocked, !changeNavSubpStreamOnceInSameCell, 
			  DVDSetup.ShowSubtitles,
		          isInMenuDomain, dvdnav_is_domain_vts(nav));
	      }
	      break;
	  }
	  case DVDNAV_AUDIO_STREAM_CHANGE: {
	      DEBUG_NAV("%s:%d:NAV AUDIO STREAM CHANGE\n", __FILE__, __LINE__);
	      dvdnav_audio_stream_change_event_t *ev;
	      ev = (dvdnav_audio_stream_change_event_t *)cache_ptr;
	      if( ! currentNavAudioTrackUsrLocked )
	      {
		      currentNavAudioTrack = dvdnav_get_active_audio_stream(nav);
		      DEBUG_AUDIO_ID("DVDNAV_AUDIO_STREAM_CHANGE: curNavAu=%d 0x%X, phys=%d, 0x%X\n", 
			  currentNavAudioTrack, currentNavAudioTrack, ev->physical, ev->physical);
	      } else {
		      DEBUG_AUDIO_ID("DVDNAV_AUDIO_STREAM_CHANGE: ignore (locked) phys=%d, 0x%X\n", 
			  ev->physical, ev->physical);
	      }
	      break;
	  }
	  case DVDNAV_VTS_CHANGE:
	      DEBUG_NAV("%s:%d:NAV VTS CHANGE -> Empty(false), setAll-spu&audio \n", __FILE__, __LINE__);
	      Empty(false); // do not clear the device !
	      setAllSubpStreams();
              setAllAudioTracks();
	      SetTitleInfoString();
	      SetTitleString();
	      SetAspectString();
	      break;
	  case DVDNAV_CELL_CHANGE: {
	      DEBUG_NAV("%s:%d:NAV CELL CHANGE\n", __FILE__, __LINE__);
	      dvdnav_cell_change_event_t * cell_info = (dvdnav_cell_change_event_t *)cache_ptr;

	      /**
	       * update information 
	       */
	      lastCellEventInfo = *cell_info;
	      int dummy;
              (void) GetBlockIndex(dummy, pgcTotalBlockNum);
	      BlocksToPGCTicks( 1, pgcTotalBlockNum, 
    			        pgcTicksPerBlock, pgcTotalTicks);
#ifdef CTRLDEBUG
	      printCellInfo(lastCellEventInfo, pgcTicksPerBlock, pgcTotalTicks);
#endif
	      //did the old cell end in a still frame?
	      SendIframe( stillFrame & CELL_STILL );
	      stillFrame = (dvdnav_get_next_still_flag(nav) != 0) ? CELL_STILL : 0;
	      // cell change .. game over ..
	      changeNavSubpStreamOnceInSameCell=false;
    	      SetTitleInfoString();
	      break;
	  }
	  case DVDNAV_NAV_PACKET: {
	      DEBUG_NAV0("%s:%d:NAV PACKET\n", __FILE__, __LINE__);
	      dsi_t *dsi;
	      current_pci = dvdnav_get_current_nav_pci(nav);
	      DEBUG_NAV0("NAV: %x, prev_e_ptm: %8d, s_ptm: %8d, ", stillFrame, prev_e_ptm,
		     current_pci->pci_gi.vobu_s_ptm);
	      if (prev_e_ptm)
		  ptm_offs += (prev_e_ptm - current_pci->pci_gi.vobu_s_ptm);
	      DEBUG_NAV0("ptm_offs: %8d\n", ptm_offs);
	      prev_e_ptm = current_pci->pci_gi.vobu_e_ptm;
	      if (current_pci && (current_pci->hli.hl_gi.hli_ss & 0x03) == 1)
		  UpdateButtonHighlight(NULL);
	      dsi = dvdnav_get_current_nav_dsi(nav);
	      SendIframe( stillFrame & NAV_STILL );
	      if (dsi->vobu_sri.next_video == 0xbfffffff)
		  stillFrame |= NAV_STILL;
	      else
		  stillFrame &= ~NAV_STILL;
	      break;
	      
	  }
	  case DVDNAV_STOP:
	      DEBUG_NAV("%s:%d:NAV STOP\n", __FILE__, __LINE__);
	      running = false;
	      break;
	  case DVDNAV_HIGHLIGHT:
	  {
	      DEBUG_NAV("%s:%d:NAV HIGHLIGHT\n", __FILE__, __LINE__);
	      dvdnav_highlight_event_t *hlevt = (dvdnav_highlight_event_t *)cache_ptr;
	      UpdateButtonHighlight(hlevt);
	      break;
	  }
	  case DVDNAV_SPU_CLUT_CHANGE:
	      DEBUG_NAV("%s:%d:NAV SPU CLUT CHANGE SPUdecoder=%d\n", 
			__FILE__, __LINE__, SPUdecoder!=NULL);
	      if (SPUdecoder)
	      {
	  	  ClearButtonHighlight();
		  SPUdecoder->setPalette((uint32_t *)cache_ptr);
	      }
	      break;
	  case DVDNAV_HOP_CHANNEL:
	      DEBUG_NAV("%s:%d:NAV HOP CHANNEL -> Empty(false)\n", __FILE__, __LINE__);
	      Empty(false);
	      break;
	  default:
	      DEBUG_NAV("%s:%d:NAV ???\n", __FILE__, __LINE__);
	      break;
      }
#ifdef DVDDEBUG
      if ((event != DVDNAV_BLOCK_OK) &&
	  (event != DVDNAV_NAV_PACKET) &&
	  (event != DVDNAV_STILL_FRAME))
	  DEBUGDVD("got event (%d)%s, len %d\n",
		   event, dvd_ev[event], len);
#endif
      if (cache_ptr != 0 && cache_ptr != event_buf)
	  dvdnav_free_cache_block(nav, cache_ptr);
  }

  DEBUG_NAV("%s:%d: empty\n", __FILE__, __LINE__);
  Empty();
  fflush(NULL);

  active = running = false;

  SPUdecoder=NULL;

  dvdnav_close(nav);
  nav=NULL;

#if VDRVERSNUM<=10206
  dsyslog("input thread ended (pid=%d)", getpid());
#endif

  DEBUGDVD("%s:%d: input thread ended (pid=%d)", __FILE__, __LINE__, getpid());
  fflush(NULL);
}

void cDvdPlayer::ClearButtonHighlight(void)
{
    LOCK_THREAD;

    if (SPUdecoder)
    {
	    SPUdecoder->clearHighlight();
            DEBUG_NAV("DVD NAV SPU clear buttonsd\n");
    }
}


void cDvdPlayer::UpdateButtonHighlight(dvdnav_highlight_event_t *hlevt)
{
    LOCK_THREAD;

    if(hlevt) {
            DEBUG_NAV("DVD NAV SPU highlight evt: %d: %d/%d %d/%d (%dx%d)\n", 
		hlevt->buttonN, hlevt->sx, hlevt->sy, hlevt->ex, hlevt->ey, 
		hlevt->ex-hlevt->sx+1, hlevt->ey-hlevt->sy+1);
    } else {
            DEBUG_NAV("DVD NAV SPU highlight evt: NULL\n");
    }

    int buttonN;
    dvdnav_highlight_area_t hl;
    
    dvdnav_get_current_highlight(nav, &buttonN);
    if (SPUdecoder && current_pci && TakeOsd()) 
    {
	if (dvdnav_get_highlight_area(current_pci,
				      buttonN, 0, &hl) == DVDNAV_STATUS_OK) {

            DEBUG_NAV("DVD NAV SPU highlight button: %d: %d/%d %d/%d (%dx%d)\n", 
		buttonN, hl.sx, hl.sy, hl.ex, hl.ey, hl.ex-hl.sx+1, hl.ey-hl.sy+1);

            if ( (hl.ex-hl.sx+1) & 0x03 ) {
                hl.ex +=4-((hl.ex-hl.sx+1) & 0x03);
            }
            DEBUG_NAV("\t\t-> %d/%d %d/%d (%dx%d)\n", 
		hl.sx, hl.sy, hl.ex, hl.ey, hl.ex-hl.sx+1, hl.ey-hl.sy+1);
 
	    SPUdecoder->setHighlight(hl.sx, hl.sy, hl.ex, hl.ey, hl.palette);
	} else {
	    // this should never happen anyway
	    DEBUG_NAV("DVD NAV SPU clear & select 1 %s:%d\n", __FILE__, __LINE__);
	    dvdnav_button_select(nav, current_pci, 1);
	    ClearButtonHighlight();
            currButtonN = -1;
	    return;
	}
    } else {
	    DEBUG_NAV("not current pci button: %d, SPUdecoder=%d, current_pci=0x%p\n", 
	    	buttonN, SPUdecoder!=NULL, current_pci);
    }
    currButtonN = buttonN;
}

#define NO_PICTURE 0
#define SC_PICTURE 0x00

/*
 * current video parameter have been set by ScanVideoPacket,
 * update the state struct now appropriatly
 * aspect:
   	0001xxxx ==  1:1 (square pixel)
   	0010xxxx ==  4:3
   	0011xxxx == 16:9
   	0100xxxx == 2,21:1
 */
cSpuDecoder::eScaleMode cDvdPlayer::doScaleMode()
{
    int o_hsize=hsize, o_vsize=vsize, o_vaspect=vaspect;

    cSpuDecoder::eScaleMode newMode     = cSpuDecoder::eSpuNormal;
    // eVideoDisplayFormat vdf             = vaspect==2?vdfPAN_SCAN:vdfLETTER_BOX;

    int dvd_aspect = dvdnav_get_video_aspect(nav);

    int perm = dvdnav_get_video_scale_permission(nav);

    // nothing has to be done, if 
    //		TV	16:9
    // 		DVD 	 4:3
    if (!Setup.VideoFormat && dvd_aspect != 0 ) {
    	//
	// if we are here, then 
	//	TV==4:3 && DVD==16:9
	//
	if (vaspect != 2) {
	    //
	    //and the actual material on the DVD is not 4:3
	    //
	    if (!(perm & 1)) {  // letterbox is allowed, keep 16:9 and wxh
		newMode = cSpuDecoder::eSpuLetterBox;
		vaspect = 0x03;
	    } else if (!(perm & 2)) {    // pan& scan allowed ..
		newMode = cSpuDecoder::eSpuPanAndScan;
		vaspect = 0x02;   // 4:3
                // vdf = vdfCENTER_CUT_OUT;
		// hsize  = o_hsize  / ( 4 * 3 ) * 16 ; // streched hsize
		// hsize  = o_hsize  / 16 * 3 * 4; // shorten hsize
		// vsize = o_vsize /  9 * 4 * 3; // stretched vsize
		// vsize = o_vsize /  ( 3 * 4 ) * 9 ; // shorten vsize
	    }
	}
    }

    if (SPUdecoder) SPUdecoder->setScaleMode(newMode);
    // cDevice::PrimaryDevice()->SetDisplayFormat(vdf);

#if 0
    DEBUG_SUBP_ID("dvd doScaleMode: have TV %s, DVD %s(0x%x), actual material %s (0x%hx: %hux%hu) -> %s (0x%hx: %hux%hu), menu=%d, set spu scale mode: %s\n",
    	(Setup.VideoFormat==0)?"4:3":"16:9", 
	(dvd_aspect==0)?"4:3":"16:9",
	dvd_aspect,
	(o_vaspect==3)?"16:9":"4:3",
	o_vaspect, o_hsize, o_vsize,
	(vaspect==3)?"16:9":"4:3",
	vaspect, hsize, vsize,
	IsInMenuDomain(),
	(newMode==cSpuDecoder::eSpuNormal)?"normal":
		(newMode==cSpuDecoder::eSpuLetterBox)?"LetterBox":"PanAndScan" );
#endif

    (void)o_hsize;
    (void)o_vsize;
    (void)o_vaspect;

    return newMode;
}

/*
 * current video parameter have been set by ScanVideoPacket,
 * update the state struct now appropriatly
 */
cSpuDecoder::eScaleMode cDvdPlayer::getSPUScaleMode()
{
    cSpuDecoder::eScaleMode newMode     = cSpuDecoder::eSpuNormal;

    int dvd_aspect = dvdnav_get_video_aspect(nav);

    // int perm = dvdnav_get_video_scale_permission(nav);

    // IsInMenuDomain() || IsDvdNavigationForced() 

    // nothing has to be done, if we display 16:9
    // or if the DVD is 4:3
    if (!Setup.VideoFormat && dvd_aspect != 0) {
    	//
	// if we are here, then the TV is 4:3, but the DVD says it is 16:9
	//
        newMode = cSpuDecoder::eSpuLetterBox;

/**
	    if (!(perm & 1)) {  // letterbox is allowed
		newMode = cSpuDecoder::eSpuLetterBox;
	    } else if (!(perm & 2)) {    // pan& scan allowed
		newMode = cSpuDecoder::eSpuPanAndScan;
	    }
 */
    }

    DEBUG_SUBP_ID("dvd getSPUScaleMode: have TV %s, DVD %s, set spu scale mode: %s\n",
    	(Setup.VideoFormat==0)?"4:3":"16:9", 
	(dvd_aspect==0)?"4:3":"16:9",
	(newMode==cSpuDecoder::eSpuNormal)?"normal":
		(newMode==cSpuDecoder::eSpuLetterBox)?"LetterBox":"PanAndScan" );

    return newMode;
}

void cDvdPlayer::seenVPTS(uint64_t pts)
{
	seenSpuPts(pts);
}

void cDvdPlayer::seenAPTS(uint64_t pts)
{
      seenSpuPts(pts);
}

void cDvdPlayer::seenSpuPts(uint64_t pts)
{
    LOCK_THREAD;
    if (SPUdecoder && TakeOsd()) {
	SPUdecoder->setTime(pts);
    }
}

void cDvdPlayer::SendIframe(bool doSend) {
    if (IframeCnt == 0 && iframeAssembler->Available() && doSend ) {
            DEBUG_IFRAME("I-Frame: Doing StillFrame: IframeCnt=%d->1, used=%d, doSend=%d, stillFrame=0x%X\n",
            IframeCnt, iframeAssembler->Available(), doSend, stillFrame);
	    IframeCnt = 1;
	}
}

bool cDvdPlayer::playSPU(int spuId, unsigned char *data, int datalen)
{
	int spu_size = 0;
	int spuh = CMD_SPU_EOF;
	bool forcedDisplay = false;
	bool isInMenuDomain = IsInMenuDomain();

	spu_size = SPUassembler.spucmd.initSPUCommand();
	if ( spu_size <= 0 ) {
		DEBUG_SUBP_DEC("playSPU: init failed, size too less %d\n", spu_size);
		return false;
	}

	while ( (spuh=SPUassembler.spucmd.getNextSPUCommand()) != CMD_SPU_EOF )
	{
		DEBUG_SUBP_DEC("getSPUCommand: spu header: %d\n", spuh);
	
		switch(spuh) {

		    case CMD_SPU_FORCE_DISPLAY:
			DEBUG_SUBP_DEC("SPU forced\n");
			forcedDisplay = true;
			break;
		}
	}

        DEBUG_SUBP_DEC("playSPU: dec stream: id=%d, cur=%d, menu=%d, forcedSubsOnly=%d, forcedDisplay=%d\n",
	    spuId, currentNavSubpStream, isInMenuDomain, 
	    forcedSubsOnly, forcedDisplay);

/*
        DEBUG_SUBP_ID("playSPU: got stream: id=%d, cur=%d, menu=%d, forcedSubsOnly=%d, forcedDisplay=%d\n",
	    spuId, currentNavSubpStream, isInMenuDomain, 
	    forcedSubsOnly, forcedDisplay);
*/

	uint8_t *buffer = new uint8_t[SPUassembler.getSize()];
	SPUassembler.Get(buffer, SPUassembler.getSize());

        if ( DVDSetup.ShowSubtitles || isInMenuDomain || ( forcedDisplay && !isInMenuDomain ) 
	     /**
	      * the problem is: both, the letterbox and the wide spu stream,
	      * does contain the force field, so i guess its just the menu flag .. 
	      *
	      * but who knows for sure, .. so forceDisplay may be usefull outta the menu space ?
	      */
	   )
	{
	    DEBUG_SUBP_ID("playSPU: SPU proc, forcedSubsOnly:%d, forcedDisplay:%d, spu_size:%d, menuDomain=%d, pts: %12d\n",
		forcedSubsOnly, forcedDisplay, spu_size, isInMenuDomain, SPUassembler.getPts());

	    SPUdecoder->processSPU(SPUassembler.getPts(), buffer);
	    if(isInMenuDomain) seenVPTS(pktpts); // else should be seen later ..
	    return true;
	}
        delete buffer;
	return false;
}

int cDvdPlayer::playPacket(unsigned char *&cache_buf, bool trickMode, bool noAudio)
{
  int playedPacket = pktNone;
  uint64_t scr;
  uint32_t mux;
  unsigned char *sector = cache_buf;
  
  static uint64_t lapts, lvpts;
  static int adiff;
  char ptype = '-';

  //make sure we got a PS packet header
  if (!cPStream::packetStart(sector, DVD_VIDEO_LB_LEN) &&
      cPStream::packetType(sector) != 0xBA) {
      esyslog("ERROR: got unexpected packet: %x %x %x %x",
	      sector[0], sector[1], sector[2], sector[3]);
      return playedPacket;
     }

  scr = cPStream::fromSCR(sector+4) * 300 + cPStream::fromSCRext(sector+9);
  mux = cPStream::fromMUXrate(sector+11);

  int offset = 14 + cPStream::stuffingLength(sector);
  sector += offset;
  int r = DVD_VIDEO_LB_LEN - offset;
  int datalen = r;

  sector[6] &= 0x8f;
//  uchar ptsFlag=sector[7] >> 6;
  bool ptsFlag = ((sector[7] & 0xC0)==0x80);
  if (ptsFlag) {
      pktptsLast = pktpts ;
      pktpts = cPStream::fromPTS(sector + 9) + ptm_offs;
      fflush(stdout);
  }

  bool isInMenuDomain = IsInMenuDomain();

  uchar *data = sector;
    
  stcPTSLast = stcPTS ;
  int64_t rawSTC;

  if ( !SoftDeviceOutActive )
  {
	  if ( (rawSTC=cDevice::PrimaryDevice()->GetSTC())>=0 )
		stcPTS=(uint64_t)rawSTC;
  } else {
	  rawSTC=-1;
  }

  switch (cPStream::packetType(sector)) {
    case VIDEO_STREAM_S ... VIDEO_STREAM_E:
         {
           datalen = cPStream::packetLength(sector);
           //skip optional Header bytes
           datalen -= cPStream::PESHeaderLength(sector);
           data += cPStream::PESHeaderLength(sector);
           //skip mandatory header bytes
           data += 3;

	   uint8_t currentFrameType = 0;
	   bool do_copy =  (lastFrameType == I_TYPE) && 
                          !(data[0] == 0 && data[1] == 0 && data[2] == 1);
	   bool haveSequenceHeader = false;
	   bool haveSliceBeforePicture = false;
	   while (datalen > 6) 
	   {
	       if (data[0] == 0 && data[1] == 0 && data[2] == 1) 
	       {
	           int  ptype2 = cPStream::packetType(data);
		   if (ptype2 == SC_PICTURE)
		   {
                       iframeAssembler->Clear();
		       VideoPts += 3600;
		       lastFrameType = (uchar)(data[5] >> 3) & 0x07;
		       if (!currentFrameType)
		           currentFrameType = lastFrameType;
		       //
		       // in trickMode, only play assembled .. I-Frames ..
		       skipPlayVideo= lastFrameType > I_TYPE && trickMode;

		       data += 5;
		       datalen -=5;
		   } else if (ptype2 == SEQUENCE_HEADER && datalen >= 8) 
		   {
	   		   haveSequenceHeader = true;
			   data += 4;           //skip the header
			   // check the aspect ratio and correct it
			   //
			   // data stream seen as 4-bit nibbles:
			   //   0  1  2  3
			   //   ww|wh|hh|ax
		           //
			   // w width
			   // h height
			   // a aspect
			   // x any
	

			   hsize    = (int) ( data[0] & 0xff ) << 4 ; // bits 04..11
			   hsize   |= (int) ( data[1] & 0xf0 ) >> 4 ; // bits 00..03

			   vsize    = (int) ( data[1] & 0x0f ) << 8 ; // bits 08..11
			   vsize   |= (int) ( data[2] & 0xff )      ; // bits 00..07

			   vaspect  = (int) ( data[3] & 0xf0 ) >> 4 ;

			   cSpuDecoder::eScaleMode scaleMode = doScaleMode();

			   (void) scaleMode;

/**
 * we won't change width/height yet ..
			   data[0]  = (uchar) ( ( hsize & 0xff0 ) >> 4 );

			   data[1]  = 0x00; // clear w/h nibble
			   data[1] |= (uchar) ( ( hsize & 0x00f ) << 4 );

			   data[1] |= (uchar) ( ( vsize & 0xf00 ) >> 8 );
			   data[2]  = (uchar) ( ( vsize & 0x0ff )      );
 */

			   data[3] &= 0x0f;
			   data[3] |= (uchar) ( ( vaspect & 0x0f ) << 4);

			   data += 3;
		           datalen -=3;
		   } else if (ptype2 == PADDING_STREAM)
		   {
        		   DEBUGDVD("PADDING_STREAM @ %d\n", data - sector);
			   #if 0
				   DEBUGDVD("PADDING_STREAM (strip) r: %d -> %d\n",
					r, data - sector);
				   r = data - sector;
			   #endif
			   break;
		   } else if( 0x01 <= ptype2 && ptype2 <= 0xaf) 
		   {
		          if( lastFrameType > I_TYPE )
				  haveSliceBeforePicture = true;
		   }
	       }
	       data++;
	       datalen--;
	   }

	   if ( haveSliceBeforePicture && 
	        (currentFrameType <= I_TYPE || do_copy || lastFrameType <= I_TYPE) 
	      ) 
           {
                DEBUG_IFRAME2("I-Frame: haveSliceBeforePicture -> clear !\n");
	        currentFrameType = 0;
	        lastFrameType = 0xff;
		do_copy=false;
	   }

	   if ( stillFrame && (currentFrameType <= I_TYPE || do_copy)) {
                DEBUG_IFRAME2("I-Frame: Put MB .. %d+%d=", r, iframeAssembler->Available());
                iframeAssembler->Put(sector, r);
                DEBUG_IFRAME2("%d\n", iframeAssembler->Available());
	   }

	   if (ptsFlag)
	   {
	       VideoPts = lvpts = pktpts;
	       cPStream::toPTS(sector + 9, pktpts, true);
           }

           rframe = new cFrame(sector, r, ftVideo);

           playedPacket |= pktVideo;
	   seenVPTS(VideoPts);
	   ptype = 'V';
           break;
         }
    case AUDIO_STREAM_S ... AUDIO_STREAM_E: {
         lastFrameType = AUDIO_STREAM_S;
         // no sound in trick mode
         if (noAudio)
            return playedPacket;

	 if (currentNavAudioTrack != (cPStream::packetType(sector) & AudioTrackMask))
	 {
/*
	    DEBUG_AUDIO_ID("packet unasked audio stream: got=%d 0x%X (0x%X), curNavAu=%d 0x%X\n", 
		  cPStream::packetType(sector), cPStream::packetType(sector),
		  cPStream::packetType(sector) & AudioTrackMask,
		  currentNavAudioTrack, currentNavAudioTrack);
 */
            return playedPacket;
	 }

         playedPacket |= pktAudio;
         SetCurrentNavAudioTrackType(aMPEG, true);

	 if (ptsFlag) {
	     adiff = pktpts - lapts;
	     lapts = pktpts;
	     cPStream::toPTS(sector + 9, pktpts, true);
	 }

	 if(rawSTC>=0) {
		 pktptsLastAudio = pktptsAudio ;
		 stcPTSLastAudio = stcPTSAudio ;
		 pktptsAudio = pktpts;
		 stcPTSAudio = stcPTS;
	 }

 	 if ( !(playMULTICHANNEL) && 
	      ( !HasBitStreamOut || BitStreamOutActive ) )
         {
		DEBUG_AUDIO_PLAY2("dvd PlayAudio mp2 stc=%8ums apts=%8ums vpts=%8ums len=%d\n", 
			(unsigned int)(stcPTS/90U), 
			(unsigned int)(ptsFlag?pktpts/90U:0), 
			(unsigned int)(VideoPts/90U), r);
	  	PlayAudio(sector, r);
         }

	 rframe = new cFrame(sector, r, ftAudio);

	 if (ptsFlag) 
	     seenAPTS(pktpts);

	 ptype = 'M';

         break;
         }
    case PRIVATE_STREAM1:
         {
           datalen = cPStream::packetLength(sector);
           //skip optional Header bytes
           datalen -= cPStream::PESHeaderLength(sector);
           data += cPStream::PESHeaderLength(sector);
           //skip mandatory header bytes
           data += 3;
           //fallthrough is intended
         }
    case PRIVATE_STREAM2: {
         lastFrameType = PRIVATE_STREAM2;
	 //FIXME: Stream1 + Stream2 is ok, but is Stream2 alone also?

	 // no sound, no spu in trick mode
	 if (noAudio)
             return playedPacket;

	 // skip PS header bytes
	 data += 6;
	 // data now points to the beginning of the payload
	 int audioType = ((int)*data) & 0xF8;

	 if( audioType == aAC3 ||
	     audioType == aDTS ||
	     audioType == aLPCM
           )
	 {
		  ptype = 'A';
		  if (ptsFlag) {
		      adiff = pktpts - lapts;
		      lapts = pktpts;
		      cPStream::toPTS(sector + 9, pktpts, true);
		  }

		  if ( currentNavAudioTrack == (*data & AudioTrackMask) ) 
		  {
                      playedPacket |= pktAudio;
                      SetCurrentNavAudioTrackType(audioType, true);

		      if(rawSTC>=0) {
			      pktptsLastAudio = pktptsAudio ;
			      stcPTSLastAudio = stcPTSAudio ;
			      pktptsAudio = pktpts;
			      stcPTSAudio = stcPTS;
		      }

 		      if ( !(playMULTICHANNEL) && 
                           ( !HasBitStreamOut || BitStreamOutActive ) ) 
		      {
			      DEBUG_AUDIO_PLAY("dvd PlayAudio %d/0x%X stc=%8ums apts=%8ums vpts=%8ums len=%d\n", 
				audioType, audioType,
				(unsigned int)(stcPTS/90U), 
				(unsigned int)(ptsFlag?pktpts/90U:0), 
				(unsigned int)(VideoPts/90U), r);
                    //remove AC3 Header
                    int ac3offset = sector[8] + 9;
                    int ac3length = cPStream::packetLength(sector) - 4;
                    cPStream::SetPacketLength(sector, ac3length);
                    //backward to save data
                    for(int i=ac3offset-1; i>=0; i--)
                        sector[i+4] = sector[i];
                    PlayAudio(sector+4, r-4);
 		      }

		      if ( audioType == aLPCM || playMULTICHANNEL) 
		      {
                              rframe = new cFrame(sector, r, ftAudio);
			      DEBUG_AUDIO_PLAY("dvd pcm/fake stc=%8ums apts=%8ums vpts=%8ums len=%d\n", 
				(unsigned int)(stcPTS/90U), 
				(unsigned int)(ptsFlag?pktpts/90U:0), 
				(unsigned int)(VideoPts/90U), r);
		              if (ptsFlag) 
			         seenAPTS(pktpts);
		      } else if ( audioType == aAC3 && !BitStreamOutActive ) {
			      data += 4;
			      // header: 3 (mandatory) + 6 (PS) + 4 (AC3)
			      datalen -= 13;
			      DEBUG_AUDIO_PLAY("dvd a52dec stc=%8ums apts=%8ums vpts=%8ums len=%d\n", 
				(unsigned int)(stcPTS/90U), 
				(unsigned int)(ptsFlag?pktpts/90U:0), 
				(unsigned int)(VideoPts/90U), datalen);
		              if (ptsFlag && a52dec.getSyncMode()==A52decoder::ptsCopy)
			        seenAPTS(pktpts);
			      a52dec.decode(data, datalen, pktpts);
		      } else if (audioType == aDTS && !BitStreamOutActive ) {
			      // todo DTS ;-)
			      DEBUG_AUDIO_PLAY("dvd aDTS n.a.\n");
		              if (ptsFlag) 
			         seenAPTS(pktpts);
		      }
		  }
	 } 
	 else if ( ((int)*data & 0xE0) == 0x20 )
	 {
	          int thisSpuId = (int)(*data) & SubpStreamMask;
		  ptype = 'S';

		  if ( OsdInUse() ) 
		  {
		      /**
		       * somebody uses the osd ..
		      DEBUG_SUBP_ID("SPU in vts ignored -> OsdInUse() !\n");
		       */
                      break;
		  }

                  if (!isInMenuDomain && !IsDvdNavigationForced() && 
		      !DVDSetup.ShowSubtitles && !currentNavSubpStreamUsrLocked)
		  {
		      /**
		       * not within a menu .. no show subtitles .. no direct subtitles ..
			*/
		      DEBUG_SUBP_ID("SPU in vts ignored -> currentNavSubpStreamUsrLocked=%d, DVDSetup.ShowSubtitles=%d\n",
				currentNavSubpStreamUsrLocked, DVDSetup.ShowSubtitles);
                      break;
	          }

		  data++;
		  datalen -= 10; // 3 (mandatory header) + 6 (PS header)

                  if ( currentNavSubpStream != -1 &&  thisSpuId == currentNavSubpStream ) 
		  {
                
		    SPUassembler.Put(data, datalen, pktpts);

	            if ( SPUdecoder && SPUassembler.ready() ) 
		    {
    		        if( ! playSPU(thisSpuId, data, datalen) ) 
			{

                        /**
                         *
                        DEBUG_SUBP_ID("packet not shown spu stream: got=%d, cur=%d, SPUdecoder=%d, menu=%d, feature=%d, forcedSubsOnly=%d\n",
                            thisSpuId, currentNavSubpStream, SPUdecoder!=NULL,
                            isInMenuDomain, dvdnav_is_domain_vts(nav), forcedSubsOnly);
                         */
                        }
	            }
            }
	 } else {
                    DEBUGDVD("PRIVATE_STREAM2 unhandled (a)id: %d 0x%X\n",
                        (int)(*data), (int)(*data));
	 }
        } /* PRIVATE_STREAM2 */
        break;
    case PADDING_STREAM:
        DEBUGDVD("PADDING_STREAM ...\n");
        lastFrameType = PADDING_STREAM;
        break;
    case SYSTEM_HEADER:
    case PROG_STREAM_MAP:
    default:
         {
           lastFrameType = 0xff;
           esyslog("ERROR: don't know what to do - packetType: %x",
		   cPStream::packetType(sector));
           DEBUGDVD("ERROR: don't know what to do - packetType: %x",
		   cPStream::packetType(sector));
           return playedPacket;
         }
    }
    if( rframe && ringBuffer->Put(rframe) ) rframe=0;

#ifdef PTSDEBUG
  if (playMode != pmPlay)
      DEBUGPTS("SCR: %8Lx,%8Ld, %8d, %c %1d, %8d (%8d) - %8d (%8d)\n",
	       scr / 300, scr % 300, mux, ptype,
      	       ptsFlag, VideoPts, lvpts, lapts, adiff);
#endif
     return playedPacket;
}

int cDvdPlayer::Resume(void)
{
  return -1;
}

bool cDvdPlayer::Save(void)
{
  return false;
}

void cDvdPlayer::Activate(bool On)
{
  DEBUGDVD("cDvdPlayer::Activate %d->%d\n", active, On);

  if (On)
  {
      Start();
  } else if (active) 
  {
     if(nav)
	dvdnav_stop(nav);
     else
        running = false;
     Cancel(3);
     running = false;
     active = false;
  }
}

void cDvdPlayer::Pause(void)
{
  if(!DVDActiveAndRunning()) return;
  if (playMode == pmPause || playMode == pmStill) {
     Play();
  } else {
     LOCK_THREAD;
     if (playMode == pmFast || (playMode == pmSlow && playDir == pdBackward))
     {
	DEBUG_NAV("%s:%d: empty\n", __FILE__, __LINE__);
        Empty();
     }
     // DeviceFreeze();
     DeviceClear();
     playMode = pmPause;
  }
}

void cDvdPlayer::Stop(void)
{
  if(!DVDActiveAndRunning()) return;

  if(running && nav) {
	dvdnav_stop(nav);
	usleep( 500 * 1000 ) ;  // 500ms
  }
  if ( running ) {
	running = false;
	usleep( 500 * 1000 ) ;  // 500ms
  }
}

void cDvdPlayer::Play(void)
{
  if(!DVDActiveAndRunning()) return;
  if (playMode != pmPlay) {
     LOCK_THREAD;
     if (playMode == pmStill || playMode == pmFast || (playMode == pmSlow && playDir == pdBackward))
     {
     	if(DVDSetup.ReadAHead>0)
		dvdnav_set_readahead_flag(nav, DVDSetup.ReadAHead);
        DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
        Empty();
     }
     DevicePlay();
     playMode = pmPlay;
     playDir = pdForward;
    }
}

void cDvdPlayer::Forward(void)
{
     if(!DVDActiveAndRunning()) return;
     switch (playMode) {
       case pmFast:
            if (Setup.MultiSpeedMode) {
               TrickSpeed(playDir == pdForward ? 1 : -1);
               break;
               }
            else if (playDir == pdForward) {
               Play();
               break;
               }
            // run into pmPlay
       case pmPlay: {
            LOCK_THREAD;
	    DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
            Empty();
            DeviceMute();
            playMode = pmFast;
            playDir = pdForward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? 1 : MAX_SPEEDS);
            }
            break;
       case pmSlow:
            if (Setup.MultiSpeedMode) {
               TrickSpeed(playDir == pdForward ? -1 : 1);
               break;
               }
            else if (playDir == pdForward) {
               Pause();
               break;
               }
            // run into pmPause
       case pmStill:
       case pmPause:
            DeviceMute();
            playMode = pmSlow;
            playDir = pdForward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? -1 : -MAX_SPEEDS);
            break;
       }
}

void cDvdPlayer::Backward(void)
{
     if(!DVDActiveAndRunning()) return;
     switch (playMode) {
       case pmFast:
            if (Setup.MultiSpeedMode) {
               TrickSpeed(playDir == pdBackward ? 1 : -1);
               break;
               }
            else if (playDir == pdBackward) {
               Play();
               break;
               }
            // run into pmPlay
       case pmPlay: {
            LOCK_THREAD;
	    DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
            Empty();
            DeviceMute();
            playMode = pmFast;
            playDir = pdBackward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? 1 : MAX_SPEEDS);
            }
            break;
       case pmSlow:
            if (Setup.MultiSpeedMode) {
               TrickSpeed(playDir == pdBackward ? -1 : 1);
               break;
               }
            else if (playDir == pdBackward) {
               Pause();
               break;
               }
            // run into pmPause
       case pmStill:
       case pmPause: {
            LOCK_THREAD;
	    DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
            Empty();
            DeviceMute();
            playMode = pmSlow;
            playDir = pdBackward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? -1 : -MAX_SPEEDS);
            }
            break;
       }
}

int cDvdPlayer::GetProgramNumber() const 
{
	return lastCellEventInfo.pgN;
}

int cDvdPlayer::GetCellNumber() const 
{
	return lastCellEventInfo.cellN;
}

int64_t cDvdPlayer::GetPGLengthInTicks() 
{
	return lastCellEventInfo.pg_length;
}

int64_t cDvdPlayer::GetPGCLengthInTicks() 
{
	return lastCellEventInfo.pgc_length;
}

void cDvdPlayer::BlocksToPGCTicks( int CurrentBlockNum, int TotalBlockNum, 
    			           int64_t & CurrentTicks, int64_t & TotalTicks ) 
{
  if(!DVDActiveAndRunning()) { CurrentTicks=-1; TotalTicks=-1; return; }
  if (TotalBlockNum==-1)
        TotalBlockNum=(int)pgcTotalBlockNum;
  TotalTicks = GetPGCLengthInTicks();
  CurrentTicks = CurrentBlockNum * (TotalTicks / TotalBlockNum) ;
}


void cDvdPlayer::PGCTicksToBlocks( int64_t CurrentTicks, int64_t TotalTicks,
			 	   int &CurrentBlockNum, int &TotalBlockNum) 
{
  if(!DVDActiveAndRunning()) { CurrentBlockNum=-1; TotalBlockNum=-1; return; }
  uint32_t pos, len;
  if(TotalTicks==-1) TotalTicks = GetPGCLengthInTicks();
  dvdnav_set_PGC_positioning_flag ( nav, 1);
  if( dvdnav_get_position ( nav, &pos, &len) == DVDNAV_STATUS_OK)
  {
        TotalBlockNum=(int)len;
        CurrentBlockNum = CurrentTicks * TotalBlockNum / TotalTicks ;
	return;
  }
  CurrentBlockNum = -1;
  TotalBlockNum = -1;
}


bool cDvdPlayer::GetIndex(int &CurrentFrame, int &TotalFrame, bool SnapToIFrame) 
{
    int TotalBlockNum, CurrentBlockNum;
    int64_t CurrentTicks, TotalTicks;

    if(!DVDActiveAndRunning()) return false;
    if ( ! GetBlockIndex ( CurrentBlockNum, TotalBlockNum ) ) return false;

    BlocksToPGCTicks( CurrentBlockNum, TotalBlockNum, 
		      CurrentTicks, TotalTicks );

    CurrentFrame = ( CurrentTicks / 90000L ) * FRAMESPERSEC ; 
    TotalFrame   = ( TotalTicks   / 90000L ) * FRAMESPERSEC ; 

    return true;
}

bool cDvdPlayer::GetBlockIndex(int &CurrentBlockNum, int &TotalBlockNum, bool SnapToIFrame) 
{
    dvdnav_status_t res;
    uint32_t pos, len;

    TotalBlockNum=-1; CurrentBlockNum=-1;
    if(!DVDActiveAndRunning()) return false;

    dvdnav_set_PGC_positioning_flag ( nav, 1);
    res = dvdnav_get_position ( nav, &pos, &len);
    if(res!=DVDNAV_STATUS_OK) 
    {
        DEBUG_CONTROL("dvd get pos in title failed ..\n");
    	return false;
    }

    TotalBlockNum=(int)len;
    CurrentBlockNum=(int)pos;

    return true;
}

int cDvdPlayer::SkipFrames(int Frames)
{
        if(!DVDActiveAndRunning()) return -1;
	SkipSeconds(Frames*FRAMESPERSEC);
	return Frames;
}

void cDvdPlayer::SkipSeconds(int Seconds)
{
  if(!DVDActiveAndRunning()) return;
  if (Seconds) {
      int64_t DiffTicks = Seconds * 90000;
      int DiffBlockNum, CurrentBlockNum, TotalBlockNum;

      if( ! GetBlockIndex ( CurrentBlockNum, TotalBlockNum ) ) return;

      PGCTicksToBlocks( DiffTicks, -1, DiffBlockNum, TotalBlockNum);

      CurrentBlockNum += DiffBlockNum;
    
      LOCK_THREAD;
      DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
      Empty();

      if ( dvdnav_sector_search( nav, CurrentBlockNum, SEEK_SET) != 
	   DVDNAV_STATUS_OK )
	printf("dvd error dvdnav_sector_search: %s\n", 
		dvdnav_err_to_string(nav));

      Play();
  }
}

bool cDvdPlayer::GetPositionInSec(int64_t &CurrentSec, int64_t &TotalSec) 
{
      int64_t CurrentTicks=0, TotalTicks=0; 

      if(!DVDActiveAndRunning()) { CurrentSec=-1; TotalSec=-1; return false; }

      if( ! GetPositionInTicks(CurrentTicks, TotalTicks) ) return false;

      CurrentSec = PTSTicksToSec(CurrentTicks);
      TotalSec   = PTSTicksToSec(TotalTicks);

      return true;
}

bool cDvdPlayer::GetPositionInTicks(int64_t &CurrentTicks, int64_t &TotalTicks) 
{
      int CurrentBlockNum, TotalBlockNum;

      if(!DVDActiveAndRunning()) { CurrentTicks=-1; TotalTicks=-1; return false; }

      if( ! GetBlockIndex ( CurrentBlockNum, TotalBlockNum ) ) return false;

      BlocksToPGCTicks( CurrentBlockNum, TotalBlockNum, CurrentTicks, TotalTicks );
      
      return true;
}

void cDvdPlayer::Goto(int Seconds, bool Still)
{
      int64_t CurrentTicks = Seconds * 90000;
      int CurrentBlockNum, TotalBlockNum;

      if(!DVDActiveAndRunning()) return;

      PGCTicksToBlocks( CurrentTicks, -1, CurrentBlockNum, TotalBlockNum);

      LOCK_THREAD;
      DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
      Empty();

      if ( dvdnav_sector_search( nav, CurrentBlockNum, SEEK_SET) != 
	   DVDNAV_STATUS_OK )
	printf("dvd error dvdnav_sector_search: %s\n", 
		dvdnav_err_to_string(nav));

      Play();
}

int cDvdPlayer::GotoAngle(int angle)
{
      int num_angle = -1, cur_angle = -1;

      if(!DVDActiveAndRunning()) return -1;
      LOCK_THREAD;
      DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
      Empty();

      dvdnav_get_angle_info(nav, &cur_angle, &num_angle);

      if(angle>num_angle) angle=1;
      if(angle<=0) angle=num_angle;

      if (stillTimer == 0)
	      dvdnav_angle_change(nav, angle);

      Play();

      return angle;
}

void cDvdPlayer::NextAngle()
{
      int num_angle = -1, cur_angle = -1;

      if(!DVDActiveAndRunning()) return;
      dvdnav_get_angle_info(nav, &cur_angle, &num_angle);
      (void) GotoAngle(++cur_angle);
}

int cDvdPlayer::GotoTitle(int title)
{
      int titleNumber;
      if(!DVDActiveAndRunning()) return -1;
      LOCK_THREAD;
      DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
      Empty();

      dvdnav_get_number_of_titles(nav, &titleNumber);

      if(title>titleNumber) title=1;
      if(title<=0) title=titleNumber;

      if (stillTimer == 0)
      {
	      dvdnav_part_play(nav, title, 1);
	      // dvdnav_title_play(nav, title);
      }

      Play();

      return title;
}

void cDvdPlayer::NextTitle()
{
      int titleNo, chapterNo;

      if(!DVDActiveAndRunning()) return ;
      dvdnav_current_title_info(nav, &titleNo, &chapterNo);
      (void) GotoTitle(++titleNo);
}

void cDvdPlayer::PreviousTitle()
{
      int titleNo, chapterNo;

      if(!DVDActiveAndRunning()) return;
      dvdnav_current_title_info(nav, &titleNo, &chapterNo);
      (void) GotoTitle(--titleNo);
}

int cDvdPlayer::GotoPart(int part)
{
      int titleNo, chapterNumber, chapterNo;
      if(!DVDActiveAndRunning()) return -1;
      LOCK_THREAD;
      DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
      Empty();

      dvdnav_current_title_info(nav, &titleNo, &chapterNo);
      dvdnav_get_number_of_parts(nav, titleNo, &chapterNumber);

      if(part>chapterNumber) part=1;
      if(part<=0) part=chapterNumber;

      if (stillTimer == 0)
	      dvdnav_part_play(nav, titleNo, part);

      Play();

      return part;
}

void cDvdPlayer::NextPart()
{
      int titleNo, chapterNo;

      if(!DVDActiveAndRunning()) return;
      dvdnav_current_title_info(nav, &titleNo, &chapterNo);
      (void) GotoPart(++chapterNo);
}

void cDvdPlayer::PreviousPart()
{
      int titleNo, chapterNo;

      if(!DVDActiveAndRunning()) return;
      dvdnav_current_title_info(nav, &titleNo, &chapterNo);
      (void) GotoPart(--chapterNo);
}

void cDvdPlayer::NextPG()
{
      if(!DVDActiveAndRunning()) return;
      LOCK_THREAD;
      DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
      Empty();
      if (stillTimer == 0)
	      dvdnav_next_pg_search(nav);
      Play();
}

void cDvdPlayer::PreviousPG()
{
      if(!DVDActiveAndRunning()) return;
      LOCK_THREAD;
      DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
      Empty();
      dvdnav_prev_pg_search(nav);
      Play();
}

void cDvdPlayer::clearSeenSubpStream( )
{
        LOCK_THREAD;

	navSubpStreamSeen.Clear();
	currentNavSubpStream=-1;
	notifySeenSubpStream(-1); // no subp ..
	DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: seen subp cleared\n");
        SetCurrentNavSubpStreamUsrLocked(false);
}

void cDvdPlayer::setAllSubpStreams(void)
{
    clearSeenSubpStream();

    uint16_t lang_code1;

    for(int i = 0; nav!=NULL && i < MaxSubpStreams; i++) {
      lang_code1=GetNavSubpStreamLangCode( i );
      if(lang_code1!=0xffff) {
	notifySeenSubpStream( i );
      }
    }
}

void cDvdPlayer::notifySeenSubpStream( int navSubpStream )
{
    int i=navSubpStreamSeen.Count()-1;

    while ( i >= 0)
    {
	if ( navSubpStream == ((IntegerListObject *)navSubpStreamSeen.Get(i))->getValue() )
		break; // found
	i--;
    }

    if ( i < 0 )
    {
#ifdef SUBPDEBUG
	    char buff2[12];
	    int channel, channel_active;
            uint16_t lang_code1;
            const char * p1 = (char *)&lang_code1;

	    channel = navSubpStream;
	    buff2[0]=0;
	    if(nav!=NULL) 
	    {
		    lang_code1=GetNavSubpStreamLangCode( channel );
		    if(lang_code1!=0xffff) {
				buff2[0]=p1[1];
				buff2[1]=p1[0];
				buff2[2]=0;
		    }
	    	    channel_active = dvdnav_get_active_spu_stream(nav);
	    }
            printf("cDvdPlayer::cDvdPlayer: seen new subp id: 0x%X (%d), <%s>; 0x%X (%d)\n", 
			channel, channel, buff2, channel_active, channel_active); 
#endif

	    navSubpStreamSeen.Add(new IntegerListObject(navSubpStream));
    }
}
    
bool cDvdPlayer::GetCurrentNavSubpStreamUsrLocked(void) const 
{ return currentNavSubpStreamUsrLocked; }

void cDvdPlayer::SetCurrentNavSubpStreamUsrLocked(bool lock)
{ 
  currentNavSubpStreamUsrLocked=lock; 
  DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: currentNavSubpStreamUsrLocked=%s\n",
  	currentNavSubpStreamUsrLocked?"true":"false");
}

int  cDvdPlayer::GetCurrentNavSubpStream(void) const 
{ return currentNavSubpStream; }

int  cDvdPlayer::GetCurrentNavSubpStreamIdx(void) const 
{ 
    int i=navSubpStreamSeen.Count()-1;

    while ( i >= 0)
    {
	if ( currentNavSubpStream == ((IntegerListObject *)navSubpStreamSeen.Get(i))->getValue() )
		break; // found
	i--;
    }
    return i;
}

uint16_t cDvdPlayer::GetNavSubpStreamLangCode(int channel) const
{
	uint16_t lang_code1=0xFFFF;
	if(nav!=NULL) {
		int logchan=dvdnav_get_spu_logical_stream(nav, channel);
		lang_code1=dvdnav_spu_stream_to_lang( nav, logchan>=0?logchan:channel );
	}
	return lang_code1;
}

uint16_t cDvdPlayer::GetCurrentNavSubpStreamLangCode(void) const 
{
	return GetNavSubpStreamLangCode(currentNavSubpStream);
}

int cDvdPlayer::GetNavSubpStreamNumber (void) const 
{
    return navSubpStreamSeen.Count();
}

int cDvdPlayer::SetSubpStream(int id)
{
    static char buffer[10];
    uint16_t lang_code1;
    const char * p1 = (char *)&lang_code1;
    int i=navSubpStreamSeen.Count()-1;

    LOCK_THREAD;
    while ( i >= 0)
    {
	if ( id == ((IntegerListObject *)navSubpStreamSeen.Get(i))->getValue() )
		break; // found
	i--;
    }

    if( i<0 ) {
	id = ((IntegerListObject *)navSubpStreamSeen.Get(0))->getValue();
    }

    currentNavSubpStream = id;
    if(currentNavSubpStream==-1) forcedSubsOnly=true;
    SetCurrentNavSubpStreamUsrLocked(true);
    #ifdef FORCED_SUBS_ONLY_SEMANTICS
	    if(forcedSubsOnly) changeNavSubpStreamOnceInSameCell=true;
    #endif

    if( currentNavSubpStream!=-1 && nav!=NULL) {
	    lang_code1=GetCurrentNavSubpStreamLangCode();
	    sprintf(buffer,"%c%c", p1[1], p1[0]);
	    dvdnav_spu_language_select ( nav, buffer );
    } else {
	    sprintf(buffer,"auto");
    }

    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: curNavSpu next to 0x%X, idx=%d, %s, forcedSubsOnly=%d, locked=%d/%d\n",
		currentNavSubpStream, i, buffer, forcedSubsOnly, 
		currentNavSubpStreamUsrLocked, !changeNavSubpStreamOnceInSameCell);

    return id;
}

void cDvdPlayer::NextSubpStream()
{
    static char buffer[10];
    uint16_t lang_code1;
    const char * p1 = (char *)&lang_code1;
    int i=navSubpStreamSeen.Count()-1;

    LOCK_THREAD;
    while ( i >= 0)
    {
	if ( currentNavSubpStream == ((IntegerListObject *)navSubpStreamSeen.Get(i))->getValue() )
		break; // found
	i--;
    }

    if( i<0 ) {
	      MSG_ERROR(tr("Current subp stream not seen!"));
	      esyslog("ERROR: internal error current subp stream 0x%X not seen !\n",
			currentNavSubpStream);
	      return;
    }

    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: found curNavSubp(0x%X) at idx = %d\n", 
		currentNavSubpStream, i);

#ifdef FORCED_SUBS_ONLY_SEMANTICS
    //switch normal subs/ forced subs only
    if( forcedSubsOnly || currentNavSubpStream==-1 ) {
    	i = ( i + 1 ) % navSubpStreamSeen.Count();
    	forcedSubsOnly = false;
        DEBUG_SUBP_ID("SPU forcedSubsOnly=%d DISABLED\n", forcedSubsOnly);
    }
    else {
    	i = ( i ) % navSubpStreamSeen.Count();
    	forcedSubsOnly = true;
        DEBUG_SUBP_ID("SPU forcedSubsOnly=%d ENABLED\n", forcedSubsOnly);
    }
#else
    i = ( i + 1 ) % navSubpStreamSeen.Count();
#endif

    currentNavSubpStream = ((IntegerListObject *)navSubpStreamSeen.Get(i))->getValue();
    if(currentNavSubpStream==-1) forcedSubsOnly=true;

    if( currentNavSubpStream!=-1 && nav!=NULL) {
	    lang_code1=GetCurrentNavSubpStreamLangCode();
	    sprintf(buffer,"%c%c", p1[1], p1[0]);
	    dvdnav_spu_language_select ( nav, buffer );
    } else {
	    sprintf(buffer,"auto");
    }

    SetCurrentNavSubpStreamUsrLocked(true);
#ifdef FORCED_SUBS_ONLY_SEMANTICS
    if(forcedSubsOnly) changeNavSubpStreamOnceInSameCell=true;
#endif

    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: curNavSpu next to 0x%X, idx=%d, %s, forcedSubsOnly=%d, locked=%d/%d\n",
		currentNavSubpStream, i, buffer, forcedSubsOnly, 
		currentNavSubpStreamUsrLocked, !changeNavSubpStreamOnceInSameCell);
}

void cDvdPlayer::GetSubpLangCode( const char ** subplang_str ) const 
{
    static char buffer[100];
    static char buff2[20];
    uint16_t lang_code1;
    const char * p1 = (char *)&lang_code1;

    if(!DVDActiveAndRunning()) { *subplang_str="n.a."; return; }

    if( currentNavSubpStream == -1 ) {
        strcpy(buff2,"auto");
    } else {
        lang_code1=GetCurrentNavSubpStreamLangCode();

        if(lang_code1==0xffff) {
            buff2[0]=0;
        } else  {
	    buff2[0]=p1[1];
	    buff2[1]=p1[0];
#ifdef FORCED_SUBS_ONLY_SEMANTICS
	    if( forcedSubsOnly ) 
	    {
		buff2[2]='-';
		buff2[3]='a';
		buff2[4]='u';
		buff2[5]='t';
		buff2[6]='o';
		buff2[7]=0;
	    } else {
		buff2[2]=0;
	    }
#else
	    buff2[2]=0;
#endif
        }
    }

    if( GetNavSubpStreamNumber() > 2 )
        sprintf(buffer,"%s %d/%d",
            buff2, GetCurrentNavSubpStreamIdx(), GetNavSubpStreamNumber()-1);
    else if( GetNavSubpStreamNumber() > 1 )
        sprintf(buffer,"%s", buff2);
    else
        buffer[0]=0;

    *subplang_str = buffer;
}

void cDvdPlayer::clearSeenAudioTrack( )
{
	navAudioTracksSeen.Clear();
	DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: seen audio cleared\n");
        SetCurrentNavAudioTrackUsrLocked(false);
}

void cDvdPlayer::setAllAudioTracks(void)
{
    clearSeenAudioTrack();

    uint16_t lang_code1;

    for(int i = 0; nav!=NULL && i < MaxAudioTracks; i++) {
      lang_code1=GetNavAudioTrackLangCode( i );
      if(lang_code1!=0xffff) {
	notifySeenAudioTrack( i );
      }
    }
}


void cDvdPlayer::notifySeenAudioTrack( int navAudioTrack )
{
    int i=navAudioTracksSeen.Count()-1;

    while ( i >= 0)
    {
	if ( navAudioTrack == ((IntegerListObject *)navAudioTracksSeen.Get(i))->getValue() )
		break; // found
	i--;
    }

    if ( i < 0 )
    {
#ifdef SUBPDEBUG
	    char buff2[12];
	    int channel, channel_active;
            uint16_t lang_code1;
            const char * p1 = (char *)&lang_code1;

	    channel = navAudioTrack;
	    buff2[0]=0;
	    if(nav!=NULL) 
	    {
		    lang_code1=GetNavAudioTrackLangCode( channel );
		    if(lang_code1!=0xffff) {
				buff2[0]=p1[1];
				buff2[1]=p1[0];
				buff2[2]=0;
		    }
	    	    channel_active = dvdnav_get_active_audio_stream(nav);
	    }
            printf("cDvdPlayer::cDvdPlayer: seen new audio id: 0x%X (%d), <%s>; 0x%X (%d)\n", 
			channel, channel, buff2, channel_active, channel_active); 
#endif
	navAudioTracksSeen.Add(new IntegerListObject(navAudioTrack));
    }
}

bool cDvdPlayer::GetCurrentNavAudioTrackUsrLocked(void) const 
{ return currentNavAudioTrackUsrLocked; }

void cDvdPlayer::SetCurrentNavAudioTrackUsrLocked(bool lock)
{ 
  currentNavAudioTrackUsrLocked=lock; 
  DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: currentNavAudioTrackUsrLocked=%s\n",
  	currentNavAudioTrackUsrLocked?"true":"false");
}

int  cDvdPlayer::GetCurrentNavAudioTrack(void) const 
{ return currentNavAudioTrack; }

int  cDvdPlayer::GetCurrentNavAudioTrackIdx(void) const 
{ 
    int i=navAudioTracksSeen.Count()-1;

    while ( i >= 0)
    {
	if ( currentNavAudioTrack == ((IntegerListObject *)navAudioTracksSeen.Get(i))->getValue() )
		break; // found
	i--;
    }
    return i;
}

bool cDvdPlayer::SetCurrentNavAudioTrackType(int atype, bool clearDevice)
{
	lastNavAudioTrackType = currentNavAudioTrackType;
	currentNavAudioTrackType = atype;
	bool ch = lastNavAudioTrackType!=currentNavAudioTrackType;
	if(ch) {
		DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: atype 0x%X -> 0x%X := %d :: clear %d\n",
			lastNavAudioTrackType, currentNavAudioTrackType, ch, ch&&clearDevice);
	}
	if(ch&&clearDevice) {
		// FIXME ?? DeviceClear();
		// FIXME ?? DevicePlay();
	}
	return ch;
}

int  cDvdPlayer::GetCurrentNavAudioTrackType(void) const 
{
    return  currentNavAudioTrackType; // aAC3, aDTS, aLPCM, aMPEG
}

uint16_t cDvdPlayer::GetNavAudioTrackLangCode(int channel) const
{
	uint16_t lang_code1=0xFFFF;
	if(nav!=NULL) {
	        int logchan=dvdnav_get_audio_logical_stream( nav, channel );
	        lang_code1=dvdnav_audio_stream_to_lang( nav, logchan>=0?logchan:channel );
	}
	return lang_code1;
}

uint16_t cDvdPlayer::GetCurrentNavAudioTrackLangCode(void) const 
{
	return GetNavAudioTrackLangCode(currentNavAudioTrack);
}

int cDvdPlayer::GetNavAudioTrackNumber (void) const 
{
    return navAudioTracksSeen.Count();
}

int cDvdPlayer::SetAudioID(int id)
{
    static char buffer[10];
    uint16_t lang_code1;
    const char * p1 = (char *)&lang_code1;
    int i=navAudioTracksSeen.Count()-1;

    if(!DVDActiveAndRunning()) return -1;

    LOCK_THREAD;

    while ( i >= 0)
    {
	if ( id == ((IntegerListObject *)navAudioTracksSeen.Get(i))->getValue() )
		break; // found
	i--;
    }

    if( i<0 ) {
	id = ((IntegerListObject *)navAudioTracksSeen.Get(0))->getValue();
    }

    SetCurrentNavAudioTrackUsrLocked(true);
    currentNavAudioTrack = id;

    lang_code1=GetCurrentNavAudioTrackLangCode();

    sprintf(buffer,"%c%c", p1[1], p1[0]);
    dvdnav_audio_language_select(nav, buffer);

    Empty();

    DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: curNavAu set to 0x%X, %s\n", 
		currentNavAudioTrack, buffer);

    return id;
}

void cDvdPlayer::NextAudioID()
{
    static char buffer[10];
    uint16_t lang_code1;
    const char * p1 = (char *)&lang_code1;
    int i=navAudioTracksSeen.Count()-1;

    if(!DVDActiveAndRunning()) return ;

    LOCK_THREAD;

    while ( i >= 0)
    {
	if ( currentNavAudioTrack == ((IntegerListObject *)navAudioTracksSeen.Get(i))->getValue() )
		break; // found
	i--;
    }

    if( i<0 ) {
	      MSG_ERROR(tr("Current audio track not seen!"));
	      esyslog("ERROR: internal error current audio track 0x%X not seen !\n",
			currentNavAudioTrack);
	      return;
    }

    DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: found curNavAu(0x%X) at idx = %d\n", 
		currentNavAudioTrack, i);

    Empty();

    SetCurrentNavAudioTrackUsrLocked(true);
    i = ( i + 1 ) % navAudioTracksSeen.Count();
    currentNavAudioTrack = ((IntegerListObject *)navAudioTracksSeen.Get(i))->getValue();

    lang_code1=GetCurrentNavAudioTrackLangCode();

    sprintf(buffer,"%c%c", p1[1], p1[0]);
    dvdnav_audio_language_select(nav, buffer);

    DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: curNavAu next to 0x%X, %s\n", 
		currentNavAudioTrack, buffer);
}

const char **cDvdPlayer::GetAudioTracks(int *CurrentTrack) const 
{
  if (NumAudioTracks()) {
     if (CurrentTrack)
        *CurrentTrack = currentNavAudioTrack & 0x0F ;
     static const char *audioTracks1[] = { "Audio 1", NULL };
     static const char *audioTracks2[] = { "Audio 1", "Audio 2", NULL };
     static const char *audioTracks3[] = { "Audio 1", "Audio 2", "Audio 3", NULL };
     static const char *audioTracks4[] = { "Audio 1", "Audio 2", "Audio 3", "Audio 4", NULL };
     switch (NumAudioTracks()) {
	case 0: return NULL;
	case 1: return audioTracks1;
	case 2: return audioTracks2;
	case 3: return audioTracks3;
	case 4: return audioTracks4;
	default: 
	        return audioTracks4;
     }
  }
  return NULL;
}

char * cDvdPlayer::GetTitleString() const 
{
	return strdup(title_str);
}

void cDvdPlayer::SetTitleString()
{
	static const char * title_holder = NULL;
        dvdnav_status_t res;

	if(title_str) { free(title_str); title_str=0; }

        if(!DVDActiveAndRunning()) { title_str=strdup(dummy_title); return; }

	res=dvdnav_get_title_string(nav, &title_holder);
        if(res==DVDNAV_STATUS_OK)
	{
        	title_str = strdup(title_holder);
	} else {
		title_str = strdup(dummy_title);
	}
}

char * cDvdPlayer::GetTitleInfoString() const 
{
	return strdup(titleinfo_str);
}

void cDvdPlayer::SetTitleInfoString()
{
	int titleNumber=-1, titleNo=-1, chapterNumber=-1, chapterNo=-1;
	int num_angle = -1, cur_angle = -1;

	if(titleinfo_str) { free(titleinfo_str); titleinfo_str=0; }

        if(!DVDActiveAndRunning()) { titleinfo_str=strdup(dummy_title); return; }

	dvdnav_get_number_of_titles(nav, &titleNumber);
  	dvdnav_current_title_info(nav, &titleNo, &chapterNo);
        dvdnav_get_number_of_parts(nav, titleNo, &chapterNumber);

  	if(titleNo >= 1) {
    		/* no menu here */
    		/* Reflect angle info if appropriate */
    		dvdnav_get_angle_info(nav, &cur_angle, &num_angle);
    	}

	if(num_angle>1)
		asprintf(&titleinfo_str, "%d/%d %d/%d %d/%d", 
			titleNo, titleNumber,  chapterNo, chapterNumber,
			cur_angle, num_angle);
	else
		asprintf(&titleinfo_str, "%d/%d %d/%d", 
			titleNo, titleNumber,  chapterNo, chapterNumber);

        return;
}

void cDvdPlayer::GetAudioLangCode( const char ** audiolang_str ) const 
{
	static char buffer[100];
	static char buff2[4];
	char * audioTypeDescr = NULL;
	uint16_t lang_code1;
	const char * p1 = (char *)&lang_code1;

        if(!DVDActiveAndRunning()) { *audiolang_str="n.a."; return; }

	lang_code1=GetCurrentNavAudioTrackLangCode();

	switch ( currentNavAudioTrackType ) 
	{
	     case aAC3:
		audioTypeDescr = "ac3"; break;
	     case aDTS:
		audioTypeDescr = "dts"; break;
	     case aLPCM:
		audioTypeDescr = "pcm"; break;
	     case aMPEG:
		audioTypeDescr = "mp2"; break;
	     default:
		audioTypeDescr = "non"; break;
	}

	if(lang_code1==0xffff)
		buff2[0]=0;
	else  {
		buff2[0]=p1[1];
		buff2[1]=p1[0];
		buff2[2]=0;
	}

	if( GetNavAudioTrackNumber() > 1 )
		sprintf(buffer,"%s %d/%d %s",
			buff2, GetCurrentNavAudioTrackIdx()+1, GetNavAudioTrackNumber(),
			audioTypeDescr);
	else
		sprintf(buffer,"%s %s", buff2, audioTypeDescr);
	
	*audiolang_str = buffer;
}

char * cDvdPlayer::GetAspectString() const 
{
	return strdup(aspect_str);
}

void cDvdPlayer::SetAspectString()
{
	int aspect;

	if(titleinfo_str) { free(aspect_str); aspect_str=0; }

        if(!DVDActiveAndRunning()) { aspect_str=strdup(dummy_n_a); return; }

	aspect=dvdnav_get_video_aspect(nav);

	switch(aspect) {
		case 0: asprintf(&aspect_str, " 4:3"); break;
		case 2: asprintf(&aspect_str, "16:9_"); break;
		case 3: asprintf(&aspect_str, "16:9"); break;
		default: aspect_str=strdup(dummy_n_a);
	}
}

bool cDvdPlayer::GetReplayMode(bool &Play, bool &Forward, int &Speed)
{
  Play = (playMode == pmPlay || playMode == pmFast);
  Forward = (playDir == pdForward);
  if (playMode == pmFast || playMode == pmSlow)
     Speed = Setup.MultiSpeedMode ? abs(trickSpeed - NORMAL_SPEED) : 0;
  else
     Speed = -1;
  return true;
}

void cDvdPlayer::activateButton(void)
{
    DEBUGDVD("activateButton\n");
    if (current_pci) {
      // StillSkip();
      LOCK_THREAD;
      dvdnav_button_activate(nav, current_pci);
    }
}

int cDvdPlayer::callRootMenu(void)
{
    DEBUGDVD("cDvdPlayer::callRootMenu()\n");

    StillSkip();
    LOCK_THREAD;

    SetCurrentNavAudioTrackUsrLocked(false);
    return dvdnav_menu_call(nav, DVD_MENU_Root);
}

int cDvdPlayer::callTitleMenu(void)
{
    DEBUGDVD("cDvdPlayer::callTitleMenu()\n");

    StillSkip();
    LOCK_THREAD;

    SetCurrentNavAudioTrackUsrLocked(false);
    return dvdnav_menu_call(nav, DVD_MENU_Title);
}

int cDvdPlayer::callSubpMenu(void)
{
    DEBUGDVD("cDvdPlayer::callSubpMenu()\n");

    StillSkip();
    LOCK_THREAD;

    return dvdnav_menu_call(nav, DVD_MENU_Subpicture);
}

int cDvdPlayer::callAudioMenu(void)
{
    DEBUGDVD("cDvdPlayer::callAudioMenu()\n");

    StillSkip();
    LOCK_THREAD;

    SetCurrentNavAudioTrackUsrLocked(false);
    return dvdnav_menu_call(nav, DVD_MENU_Audio);
}

