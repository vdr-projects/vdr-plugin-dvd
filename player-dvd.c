/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

//#define NDEBUG

#include <sys/time.h>
#include <string.h>
#include <stdio.h>

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
#define CTRLDEBUG
#undef DEBUG_CONTROL
#define DEBUG_CONTROL(format, args...) printf (format, ## args)
#define NAVDEBUG
#undef DEBUG_NAV
#define DEBUG_NAV(format, args...) printf (format, ## args)
#define IFRAMEDEBUG
#undef DEBUG_IFRAME
#define DEBUG_IFRAME(format, args...) printf (format, ## args)
#define SUBPDEBUG
#undef DEBUG_SUBP_ID
#define DEBUG_SUBP_ID(format, args...) printf (format, ## args)
 **
#define NAV0DEBUG
#define PTSDEBUG
#undef DEBUG_PTS
#define DEBUG_PTS(format, args...) printf (format, ## args)
#define AUDIOIDDEBUG
#undef DEBUG_AUDIO_ID
#define DEBUG_AUDIO_ID(format, args...) printf (format, ## args)
#define AUDIOPLAYDEBUG
#undef DEBUG_AUDIO_PLAY
#define DEBUG_AUDIO_PLAY(format, args...) printf (format, ## args)
 */
/*
#undef DEBUG_SPU
#define DEBUG_SPU(format, args...) esyslog(format, ## args)
*/
#ifdef NAV0DEBUG
#define DEBUG_NAV0(format, args...) printf (format, ## args)
#else
#define DEBUG_NAV0(format, args...)
#endif


// --- static stuff ------------------------------------------------------

#if defined( HAVE_AC3_OVER_DVB )
#define playMULTICHANNEL         Setup.PlayMultichannelAudio
#else
#define playMULTICHANNEL         false
#endif

#ifdef CTRLDEBUG

static void printCellInfo( dvdnav_cell_change_event_t & lastCellEventInfo,
               int64_t pgcTotalTicks, int64_t pgcTicksPerBlock )
{
    int cell_length_s = (int)(lastCellEventInfo.cell_length / 90000L) ;
    int pg_length_s = (int)(lastCellEventInfo.pg_length / 90000L) ;
    int pgc_length_s = (int)(lastCellEventInfo.pgc_length / 90000L) ;
    int cell_start_s = (int)(lastCellEventInfo.cell_start / 90000L) ;
    int pg_start_s = (int)(lastCellEventInfo.pg_start / 90000L) ;

    printf("cellN %d, pgN=%d, cell_length=%ld %d, pg_length=%ld %d, pgc_length=%ld %d, cell_start %ld %d, pg_start %ld %d, ticksPerBlock=%ld, secPerBlock=%ld\n",
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

const int cDvdPlayer::MaxAudioTracks  =  8;
const int cDvdPlayer::MaxSubpStreams  = 16;

#if VDRVERSNUM<=10206
cDvdPlayer::cDvdPlayer(void): a52dec(*this) {
#else
cDvdPlayer::cDvdPlayer(void): cThread("dvd-plugin"), a52dec(*this) {
#endif
    DEBUG("cDvdPlayer::cDvdPlayer(void)\n");
    ringBuffer=new cRingBufferFrame(VIDEOBUFSIZE);
    rframe=NULL;
    pframe=NULL;
    controller = NULL;
    osdInUse  = false;
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
    SPUassembler.spuOffsetLast = 0;

    skipPlayVideo=0;
    fastWindFactor=1;

    clearSeenSubpStream();
    clearSeenAudioTrack();

    DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: curNavAu set to 0x00\n");

    SPUdecoder = NULL;
    a52dec.SyncMode(A52decoder::ptsGenerate);
}

cDvdPlayer::~cDvdPlayer()
{
  DEBUG("destructor cDvdPlayer::~cDvdPlayer()\n");
  Detach();
  //Save();
  delete iframeAssembler;
  delete ringBuffer;
  ringBuffer=NULL;
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
      ClearButtonHighlight();
      SPUdecoder->Empty();
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
  if (stillTimer > 0) {
      //XXX dvdnav_still_skip(nav);
  }
  stillTimer = 0;
  stillFrame = 0;
  IframeCnt = 0;
  skipPlayVideo=0;
  iframeAssembler->Clear();

  if(emptyDeviceToo) {
      DeviceClear();
      DevicePlay();
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
uint32_t cDvdPlayer::time_ticks(void)
{
  static time_t t0 = 0;
  struct timeval t;
  if (gettimeofday(&t, NULL) == 0) {
     if (t0 == 0)
        t0 = t.tv_sec; // this avoids an overflow (we only work with deltas)
     return (uint32_t)(t.tv_sec - t0) * 90000U +
            (uint32_t)( t.tv_usec * 9U ) / 100U ;
     }
  return 0;
}

uint32_t cDvdPlayer::delay_ticks(uint32_t ticks)
{
  uint32_t t0 = time_ticks();
  usleep(1000); // at least one guaranteed sleep of 1ms !!
  uint32_t done = time_ticks() - t0 ;
  while ( ticks > done )
  {
    // usleep resol.  is about 19 ms
    if(ticks-done>19U*90U)
        usleep( (ticks-done)*100U/9U - 19000U );
        done = time_ticks() - t0 ;
  }
  return time_ticks() - t0 ;
}

void cDvdPlayer::Action(void) {

#if VDRVERSNUM<=10206
    dsyslog("input thread started (pid=%d)", getpid());
#endif

    unsigned char  event_buf[2048];
    memset(event_buf, 0, sizeof(event_buf));

    unsigned char *write_blk = NULL;
    int blk_size = 0;

    BitStreamOutActive   = false;
    HasBitStreamOut      = (cPluginManager::GetPlugin("bitstreamout") != NULL);

    cSetupLine *slBitStreamOutActive = NULL;

    if(HasBitStreamOut) {
        slBitStreamOutActive = cPluginDvd::GetSetupLine("active", "bitstreamout");
        if(slBitStreamOutActive!=NULL)
            BitStreamOutActive = atoi ( slBitStreamOutActive->Value() ) ? true: false ;
    }
    printf("dvd player: BitStreamOutActive=%d, HasBitStreamOut=%d (%d)\n",
    BitStreamOutActive, HasBitStreamOut, slBitStreamOutActive!=NULL);

		//resume initialisation
  	const char * diskStamp;
  	int lastTitlePlayed=-1, lastBlocksPlayed=0, lastArrayIndex=-1;
    if( setDiskStamp( &diskStamp, nav ) && DVDSetup.ResumeDisk ) {
    	checkDiskStamps( diskStamp, lastTitlePlayed, lastBlocksPlayed, lastArrayIndex);    	
		}
		else {		
			controller->setResumeValue(2);
		}
		//end resume initialisation
		
    if (dvdnav_open(&nav, const_cast<char *>(cDVD::getDVD()->DeviceName())) == DVDNAV_STATUS_ERR) {
        running = false;
        nav=0;
        ERROR_MESSAGE("Error opening DVD!");
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
    if (IsAttached())
    {
        SPUdecoder = cDevice::PrimaryDevice()->GetSpuDecoder();
        DEBUG_NAV("DVD NAV SPU clear & empty %s:%d\n", __FILE__, __LINE__);
        ClearButtonHighlight();
        SPUdecoder->Empty();
    }

    int slomoloop=0;
    uint32_t sleept_trial = 0; // in ticks !
    uint32_t sleept = 0; // in ticks !
    uint32_t sleept_done = 0; // in ticks !
    bool trickMode = false;
    bool noAudio   = false;

    uint32_t cntBlocksSkipped  = 0;
    cntBlocksPlayed = 0;

    Empty(); // cleanup ..

    running = true;
    
    //actual resume if stamp exists
    if( lastTitlePlayed != -1 ) {
    	if( askForResume(lastBlocksPlayed) ) {    			
   			if( dvdnav_title_play(nav, lastTitlePlayed) == DVDNAV_STATUS_OK )
   				Resume(lastBlocksPlayed);    			 
    	}
    }
    //end actual resume
    
    eFrameType frameType=ftUnknown;
    while( running && nav ) {

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

        if ( frameType==ftAudio )
        {
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
        if (sleept) {
            if ( sleept/90U > 1000 )
                DEBUG_PTS("\n***** WARNING >=1000ms sleep %ums\n", sleept/90U);
            sleept_done = delay_ticks(sleept);

            DEBUG_PTS("dvd loop sleep=%5ut(%3ums)/%5ut(%3ums), blk_size=%3d, skipPlayV=%d, AudioBlock=%d IframeCnt=%d stillTimer=%u\n",
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
            continue;

        LOCK_THREAD;

        trickMode = playMode == pmFast || (playMode == pmSlow && playDir == pdBackward) ;

        if ( pframe ) {
            int res = blk_size;
            if( !skipPlayVideo ) {
                if ( IframeCnt < 0 && frameType==ftVideo ) {
                    // we played an IFrame with DeviceStillPicture -> reset !
                    DEBUG_IFRAME("I-Frame: DeviceClear, DevicePlay <- prev DeviceStillPicture\n");
                    IframeCnt = 0;
//                    DeviceClear();
//                    DevicePlay();
                }
                res = PlayVideo(write_blk, blk_size);

                if ( trickMode ) {
                    DEBUG_CONTROL("PLAYED  : todo=%d, written=%d\n",
                    blk_size, res);
                }
            }
#ifdef CTRLDEBUG
            else if ( trickMode )
                printf("SKIPPED : todo=%d\n", blk_size);
#endif

            if ( res<0 ) {
                if (errno != EAGAIN && errno != EINTR) {
                    esyslog("ERROR: PlayVideo, %s (workaround activ)\n", strerror(errno));
                    printf("ERROR: PlayVideo, %s (workaround activ)\n", strerror(errno));
                }
                DEBUG_CONTROL("PLAYED zero -> Clear/Play\n");
                DeviceClear();
                DevicePlay();
                continue;
            }
            if ( res>0 ) {
                blk_size -= res;
                write_blk += res;
            }
            if ( blk_size>0 )
                sleept = 5*90U;  // 5ms*90t/ms
            else {
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
            frameType=ftUnknown;
        }
        if (IframeCnt > 0) {
            DEBUG_IFRAME("I-Frame: DeviceStillPicture: IframeCnt=%d->-1, used=%d, ", IframeCnt, iframeAssembler->Available());

            DeviceClear();
            DevicePlay();
            int iframeSize=iframeAssembler->Available();
            unsigned char *iframe=iframeAssembler->Get(iframeSize);
            if ( iframe && iframeSize>0 ) DeviceStillPicture(iframe, iframeSize);
//            IframeAssembler.Clear();
            IframeCnt = -1; // mark that we played an IFrame !
            if (blk_size <= 0 && !skipPlayVideo)
                sleept = 1*90U;  // 1ms*90t/ms

            DEBUG_IFRAME("stc=%8ums vpts=%8ums sleept=%ums\n",
                (unsigned int)(stcPTS/90U),
                (unsigned int)(VideoPts/90U), sleept/90U);
            continue;
        }

        /**
         * check is we should use the very fast forward mode
         */
        if (playDir == pdForward && playMode == pmFast && skipPlayVideo && cntBlocksPlayed>0 && fastWindFactor>1) {
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

            if( pgcPosTicks+pgcPosTicksIncr < pgcTotalTicks ) {
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
                    (long)pgcPosTicks, (long)pgcPosTicksIncr, (long)pgcPosTicks+pgcPosTicksIncr);

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
                if( slomoloop==0 ) {
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
                 *      trickspeed-NORMAL_SPEED <= MAX_SPEEDS
                 * else fast rewind
                 *      half a secound * fastWindFactor
                 */
                if ( trickSpeed-NORMAL_SPEED <= MAX_SPEEDS ) {
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
            if( dvdnav_get_position ( nav, &pos, &len) == DVDNAV_STATUS_OK && pos>posdiff ) {
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
        } else if ( playMode == pmFast || playMode == pmSlow ) {
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
            if (playDir == pdForward && pos+1 == len && pgcPosTicks>90000L*10L) {
                pgcPosTicks-=90000L*10L;

                if ( dvdnav_sector_search( nav, pgcPosTicks/pgcTicksPerBlock, SEEK_SET) != DVDNAV_STATUS_OK )
                    printf("dvd error dvdnav_sector_search: %s\n", dvdnav_err_to_string(nav));
            }
        }

        unsigned char *cache_ptr = event_buf;
        int event;
        int len;

        // from here on, continue is not allowed,
        // as it would bypass dvdnav_free_cache_block
        if (dvdnav_get_next_cache_block(nav, &cache_ptr, &event, &len) != DVDNAV_STATUS_OK) {
            ERROR_MESSAGE("Error fetching data from DVD!");
            esyslog("%s:%d: %s\n", __FILE__, __LINE__, dvdnav_err_to_string(nav));
            running = false;
            break;
        }

        noAudio   = playMode != pmPlay ;
        if(controller!=NULL)
            osdInUse = controller->Visible();

        switch (event) {
        case DVDNAV_BLOCK_OK:
            // DEBUG_NAV("%s:%d:NAV BLOCK OK\n", __FILE__, __LINE__);
            playPacket(cache_ptr, trickMode, noAudio);
            break;
        case DVDNAV_NOP:
            DEBUG_NAV("%s:%d:NAV NOP\n", __FILE__, __LINE__);
            break;
        case DVDNAV_STILL_FRAME: {
            uint32_t currentTicks = time_ticks();
            DEBUG_NAV0("%s:%d:NAV STILL FRAME, rem. stillTimer=%ut(%ums), currTicks=%ut(%ums)\n",
                __FILE__, __LINE__,
                stillTimer, stillTimer/90, currentTicks, currentTicks/90);
            dvdnav_still_event_t *still = (dvdnav_still_event_t *)cache_ptr;
            if (stillTimer != 0) {
                seenPTS(0);
                if (stillTimer > currentTicks) {
                    sleept = 40*90U;  // 40ms*90t/ms
                } else {
                    stillTimer = 0;
                    dvdnav_still_skip(nav);
                }
                DEBUG_PTS("StillTimer->Sleep: %ut(%ums)\n", sleept, sleept/90);
            } else {
                stillTimer = still->length == 0xff ? INT_MAX : currentTicks + (uint32_t)(still->length) * 90000U;
                DEBUG_PTS("Still time: %ut(%ums)\n", stillTimer, stillTimer/90);
                DeviceClear();
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
            dvdnav_spu_stream_change_event_t *ev;
            ev = (dvdnav_spu_stream_change_event_t *)cache_ptr;
            DEBUG_SUBP_ID("SPU Streams: 0x%X, 0x%X, 0x%X\n",
            ev->physical_wide, ev->physical_letterbox, ev->physical_pan_scan);

            if( IsInMenuDomain() || IsDvdNavigationForced() || ( ! currentNavSubpStreamLocked && DVDSetup.ShowSubtitles ) ) {
                cSpuDecoder::eScaleMode mode = getSPUScaleMode();

                if (mode == cSpuDecoder::eSpuLetterBox ) {
                    // TV 4:3,  DVD 16:9
                    currentNavSubpStream = ev->physical_letterbox & (MaxSubpStreams-1);
                    DEBUG_SUBP_ID("dvd choosing letterbox SPU stream: curNavSpu=%d 0x%X\n",
                        currentNavSubpStream, currentNavSubpStream);
                } else {
                    currentNavSubpStream = ev->physical_wide & (MaxSubpStreams-1);
                    DEBUG_SUBP_ID("dvd choosing 4:3 SPU stream: curNavSpu=%d 0x%X\n", currentNavSubpStream, currentNavSubpStream);
                }
            } else {
                DEBUG_SUBP_ID("DVDNAV_SPU_STREAM_CHANGE: ignore (locked=%d|not enabled=%d), menu=%d, feature=%d \n",
                    currentNavSubpStreamLocked, DVDSetup.ShowSubtitles,
                    IsInMenuDomain(), dvdnav_is_domain_vts(nav));
            }
            notifySeenSubpStream( ev->physical_wide & (MaxSubpStreams-1) );
            notifySeenSubpStream( ev->physical_letterbox & (MaxSubpStreams-1) );
            notifySeenSubpStream( ev->physical_pan_scan & (MaxSubpStreams-1) );
            break;
        }
        case DVDNAV_AUDIO_STREAM_CHANGE: {
            DEBUG_NAV("%s:%d:NAV AUDIO STREAM CHANGE\n", __FILE__, __LINE__);
            dvdnav_audio_stream_change_event_t *ev;
            ev = (dvdnav_audio_stream_change_event_t *)cache_ptr;
            if( ! currentNavAudioTrackLocked ) {
                currentNavAudioTrack = dvdnav_get_active_audio_stream(nav);
                DEBUG_AUDIO_ID("DVDNAV_AUDIO_STREAM_CHANGE: curNavAu=%d 0x%X, phys=%d, 0x%X\n",
                currentNavAudioTrack, currentNavAudioTrack, ev->physical, ev->physical);
            } else {
                DEBUG_AUDIO_ID("DVDNAV_AUDIO_STREAM_CHANGE: ignore (locked) phys=%d, 0x%X\n",
                ev->physical, ev->physical);
            }
            notifySeenAudioTrack( currentNavAudioTrack );
            break;
        }
        case DVDNAV_VTS_CHANGE:
            DEBUG_NAV("%s:%d:NAV VTS CHANGE -> Empty(false),clearSeenSubpStream\n", __FILE__, __LINE__);
            Empty(false); // do not clear the device !
            clearSeenSubpStream();
            clearSeenAudioTrack();
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
            BlocksToPGCTicks( 1, pgcTotalBlockNum, pgcTicksPerBlock, pgcTotalTicks);
#ifdef CTRLDEBUG
            printCellInfo(lastCellEventInfo, pgcTicksPerBlock, pgcTotalTicks);
#endif
            //did the old cell end in a still frame?
            SendIframe( stillFrame & CELL_STILL );
            stillFrame = (dvdnav_get_next_still_flag(nav) != 0) ? CELL_STILL : 0;
            break;
        }
        case DVDNAV_NAV_PACKET: {
            DEBUG_NAV("%s:%d:NAV PACKET\n", __FILE__, __LINE__);
            dsi_t *dsi;
            current_pci = dvdnav_get_current_nav_pci(nav);
            DEBUG_NAV0("NAV: %x, prev_e_ptm: %8d, s_ptm: %8d, ", stillFrame, prev_e_ptm,
               current_pci->pci_gi.vobu_s_ptm);
            if (prev_e_ptm)
                ptm_offs += (prev_e_ptm - current_pci->pci_gi.vobu_s_ptm);
            DEBUG_NAV0("ptm_offs: %8d\n", ptm_offs);
            prev_e_ptm = current_pci->pci_gi.vobu_e_ptm;
            if (current_pci && (current_pci->hli.hl_gi.hli_ss & 0x03) == 1)
                UpdateButtonHighlight();
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
            DEBUG_NAV("%s:%d:NAV HIGHLIGHT\n", __FILE__, __LINE__);
            UpdateButtonHighlight();
            break;
        case DVDNAV_SPU_CLUT_CHANGE:
            DEBUG_NAV("%s:%d:NAV SPU CLUT CHANGE SPUdecoder=%d\n",
                __FILE__, __LINE__, SPUdecoder!=NULL);
            if ( SPUdecoder ) {
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
        if ((event != DVDNAV_BLOCK_OK) && (event != DVDNAV_NAV_PACKET) && (event != DVDNAV_STILL_FRAME))
            DEBUG("got event (%d)%s, len %d\n", event, dvd_ev[event], len);
#endif
        if ( cache_ptr && cache_ptr!=event_buf )
            dvdnav_free_cache_block(nav, cache_ptr);
    }
		//resume get pos when exit
		if( DVDSetup.ResumeDisk && !IsInMenuDomain())
			Save(diskStamp, lastArrayIndex);	
		//resume end
		
    DEBUG_NAV("%s:%d: empty\n", __FILE__, __LINE__);
    Empty();

    fflush(NULL);

    active = running = false;

    SPUdecoder=NULL;

    dvdnav_reset(nav);
    dvdnav_close(nav);
    nav=NULL;

#if VDRVERSNUM<=10206
    dsyslog("input thread ended (pid=%d)", getpid());
#endif

    DEBUG("%s:%d: input thread ended (pid=%d)", __FILE__, __LINE__, getpid());
    fflush(NULL);
}

void cDvdPlayer::ClearButtonHighlight(void) {
    LOCK_THREAD;

    if (SPUdecoder) {
        SPUdecoder->clearHighlight();
        DEBUG_NAV("DVD NAV SPU clear buttonsd\n");
    }
}


void cDvdPlayer::UpdateButtonHighlight(void) {
    LOCK_THREAD;

    int buttonN;
    dvdnav_highlight_area_t hl;

    dvdnav_get_current_highlight(nav, &buttonN);
    if ( SPUdecoder && current_pci && !osdInUse ) {
        if ( dvdnav_get_highlight_area(current_pci, buttonN, 0, &hl) == DVDNAV_STATUS_OK ) {

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
        DEBUG_NAV("not current pci button: %d, SPUdecoder=%d, current_pci=%d\n",
            buttonN, SPUdecoder!=NULL, (int)current_pci);
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
cSpuDecoder::eScaleMode cDvdPlayer::doScaleMode() {
    int o_hsize=hsize, o_vsize=vsize, o_vaspect=vaspect;

    cSpuDecoder::eScaleMode newMode     = cSpuDecoder::eSpuNormal;
    // eVideoDisplayFormat vdf             = vaspect==2?vdfPAN_SCAN:vdfLETTER_BOX;

    int dvd_aspect = dvdnav_get_video_aspect(nav);

    int perm = dvdnav_get_video_scale_permission(nav);

    // nothing has to be done, if
    //      TV  16:9
    //      DVD      4:3
    if (!Setup.VideoFormat && dvd_aspect != 0 ) {
        //
    // if we are here, then
    //  TV==4:3 && DVD==16:9
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

void cDvdPlayer::seenPTS(uint32_t pts) {
    LOCK_THREAD;
    if (SPUdecoder)
        SPUdecoder->setTime(pts);
}

void cDvdPlayer::SendIframe(bool doSend) {
    if (IframeCnt == 0 && iframeAssembler->Available() && doSend ) {
        DEBUG_IFRAME("I-Frame: Doing StillFrame: IframeCnt=%d->1, used=%d, doSend=%d, stillFrame=0x%X\n",
            IframeCnt, iframeAssembler->Available(), doSend, stillFrame);
        IframeCnt = 1;
    }
}

void cDvdPlayer::playPacket(unsigned char *&cache_buf, bool trickMode, bool noAudio)
{
    uint64_t scr;
    uint32_t mux;
    unsigned char *sector = cache_buf;

    static uint32_t lapts, lvpts;
    static int adiff;
    char ptype = '-';

    //make sure we got a PS packet header
    if (!cPStream::packetStart(sector, DVD_VIDEO_LB_LEN) && cPStream::packetType(sector) != 0xBA) {
        esyslog("ERROR: got unexpected packet: %x %x %x %x", sector[0], sector[1], sector[2], sector[3]);
      return;
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
        cPStream::toPTS(sector + 9, pktpts, true);
        fflush(stdout);
    }

    uchar *data = sector;

    stcPTSLast = stcPTS ;
    int64_t ullh;
    if ( (ullh=cDevice::PrimaryDevice()->GetSTC())>=0 )
        stcPTS=(uint32_t)ullh;

    switch (cPStream::packetType(sector)) {
        case VIDEO_STREAM_S ... VIDEO_STREAM_E: {
            datalen = cPStream::packetLength(sector);
            //skip optional Header bytes
            datalen -= cPStream::PESHeaderLength(sector);
            data += cPStream::PESHeaderLength(sector);
            //skip mandatory header bytes
            data += 3;

            uint8_t currentFrameType = 0;
            bool do_copy =  (lastFrameType == I_TYPE) &&
                          !(data[0] == 0 && data[1] == 0 && data[2] == 1);
            while (datalen > 6) {
                if (data[0] == 0 && data[1] == 0 && data[2] == 1) {
                    if (cPStream::packetType(data) == SC_PICTURE) {
                        iframeAssembler->Clear();
                        VideoPts += 3600;
                        lastFrameType = (uchar)(data[5] >> 3) & 0x07;
                        if (!currentFrameType)
                            currentFrameType = lastFrameType;
                        //
                        // in trickMode, only play assembled .. I-Frames ..
                        skipPlayVideo= lastFrameType > I_TYPE && trickMode;

                        data += 5;
                    } else {
                        if (cPStream::packetType(data) == SEQUENCE_HEADER && datalen >= 8) {
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
                        }
                    }
                }
                data++;
                datalen--;
            }
            if ( stillFrame && (currentFrameType <= I_TYPE || do_copy)) {
                DEBUG_IFRAME("I-Frame: Put MB .. %d+%d=", r, iframeAssembler->Available());
                iframeAssembler->Put(sector, r);
                DEBUG_IFRAME("%d\n", iframeAssembler->Available());
            }

            if (ptsFlag)
                VideoPts = lvpts = pktpts;

            rframe = new cFrame(sector, r, ftVideo);

            seenPTS(VideoPts);
            ptype = 'V';
            break;
        }
        case AUDIO_STREAM_S ... AUDIO_STREAM_E: {
            // no sound in trick mode
            if (noAudio)
                return;

            notifySeenAudioTrack( cPStream::packetType(sector) & (MaxAudioTracks-1) );

            if (currentNavAudioTrack != (cPStream::packetType(sector) & (MaxAudioTracks-1))) {
/*
                DEBUG_AUDIO_ID("packet unasked audio stream: got=%d 0x%X (0x%X), curNavAu=%d 0x%X\n",
                    cPStream::packetType(sector), cPStream::packetType(sector),
                    cPStream::packetType(sector) & (MaxAudioTracks-1),
                currentNavAudioTrack, currentNavAudioTrack);
 */
                return;
            }

            currentNavAudioTrackType = aMPEG;

            if (ptsFlag) {
                adiff = pktpts - lapts;
                lapts = pktpts;
            }

            pktptsLastAudio = pktptsAudio ;
            stcPTSLastAudio = stcPTSAudio ;
            pktptsAudio = pktpts;
            stcPTSAudio = stcPTS;

            if ( !(playMULTICHANNEL) && ( !HasBitStreamOut || BitStreamOutActive ) ) {
                DEBUG_AUDIO_PLAY("dvd PlayAudio mp2 stc=%8ums apts=%8ums vpts=%8ums len=%d\n",
                    (unsigned int)(stcPTS/90U),
                    (unsigned int)(ptsFlag?pktpts/90U:0),
                    (unsigned int)(VideoPts/90U), r);
                PlayAudio(sector, r);
            }
            if (ptsFlag)
                seenPTS(pktpts);
            rframe = new cFrame(sector, r, ftAudio);

            ptype = 'M';

            break;
        }
        case PRIVATE_STREAM1: {
            datalen = cPStream::packetLength(sector);
            //skip optional Header bytes
            datalen -= cPStream::PESHeaderLength(sector);
            data += cPStream::PESHeaderLength(sector);
            //skip mandatory header bytes
            data += 3;
            //fallthrough is intended
        }
        case PRIVATE_STREAM2: {
            //FIXME: Stream1 + Stream2 is ok, but is Stream2 alone also?

            // no sound in trick mode
            if (noAudio)
                return;

            // skip PS header bytes
            data += 6;
            // data now points to the beginning of the payload
            int audioType = ((int)*data) & 0xF8;

            switch (audioType) {
                case aAC3:
                case aDTS:
                case aLPCM:
                    ptype = 'A';
                    if (ptsFlag) {
                        adiff = pktpts - lapts;
                        lapts = pktpts;
                    }

                    notifySeenAudioTrack( ((int)*data) & (MaxAudioTracks-1) );

                    if ( currentNavAudioTrack == (*data & (MaxAudioTracks-1)) ) {
                    if (ptsFlag)
                        seenPTS(pktpts);

                    currentNavAudioTrackType = audioType;

                    pktptsLastAudio = pktptsAudio ;
                    stcPTSLastAudio = stcPTSAudio ;
                    pktptsAudio = pktpts;
                    stcPTSAudio = stcPTS;

                    if ( !(playMULTICHANNEL) && ( !HasBitStreamOut || BitStreamOutActive ) ) {
                        DEBUG_AUDIO_PLAY("dvd PlayAudio ac3 stc=%8ums apts=%8ums vpts=%8ums len=%d\n",
                            (unsigned int)(stcPTS/90U),
                            (unsigned int)(ptsFlag?pktpts/90U:0),
                            (unsigned int)(VideoPts/90U), r);
                        PlayAudio(sector, r);
                    }

                    if (audioType == aLPCM || playMULTICHANNEL) {
                        rframe = new cFrame(sector, r, ftAudio);
                        DEBUG_AUDIO_PLAY("dvd pcm/fake stc=%8ums apts=%8ums vpts=%8ums len=%d\n",
                            (unsigned int)(stcPTS/90U),
                            (unsigned int)(ptsFlag?pktpts/90U:0),
                            (unsigned int)(VideoPts/90U), r);
                    } else if ( audioType == aAC3 && !BitStreamOutActive ) {
                        data += 4;
                        // header: 3 (mandatory) + 6 (PS) + 4 (AC3)
                        datalen -= 13;
                        DEBUG_AUDIO_PLAY("dvd a52dec stc=%8ums apts=%8ums vpts=%8ums len=%d\n",
                            (unsigned int)(stcPTS/90U),
                            (unsigned int)(ptsFlag?pktpts/90U:0),
                            (unsigned int)(VideoPts/90U), datalen);
                            a52dec.decode(data, datalen, pktpts);
                    } else if (audioType == aDTS && !BitStreamOutActive ) {
                        // todo DTS ;-)
                        DEBUG_AUDIO_PLAY("dvd aDTS n.a.\n");
                    }
                }
                break;
                case 0x20: {
                    int thisSpuId = (int)(*data & (MaxSubpStreams-1));
                    ptype = 'S';

                    notifySeenSubpStream ( thisSpuId );

                    if ( osdInUse ) {
                        spuInUse=false;
                        /**
                         * somebody uses the osd ..
                           DEBUG_SUBP_ID("SPU in vts ignored -> osdInUse !\n");
                         */
                        break;
                    }

                    if (!IsInMenuDomain() && !IsDvdNavigationForced() && !DVDSetup.ShowSubtitles && !currentNavSubpStreamLocked) {
                        spuInUse=false;
                        /**
                         * not within a menu .. no show subtitles .. no direct subtitles ..
                         */
                        DEBUG_SUBP_ID("SPU in vts ignored -> currentNavSubpStreamLocked=%d, DVDSetup.ShowSubtitles=%d\n",
                        currentNavSubpStreamLocked, DVDSetup.ShowSubtitles);
                        break;
                    }

                    if ( SPUdecoder && (currentNavSubpStream != -1) && ( thisSpuId == currentNavSubpStream ) ) {
                        spuInUse=true;

                        data++;
                        datalen -= 10; // 3 (mandatory header) + 6 (PS header)
                        SPUassembler.Put(data, datalen, pktpts);

                        //check for forced subs
                        if( !IsInMenuDomain() && forcedSubsOnly ) {
                            //get command sequence
                            int spuh = SPUassembler.getSPUCommand(data, datalen);

                            //code is 0x01 (play)
                            if( spuh == 1  ) {
                                spuInUse=false;
                                if (SPUassembler.ready()) {
                                    uint8_t *buffer = new uint8_t[SPUassembler.getSize()];
                                    SPUassembler.Get(buffer, SPUassembler.getSize());
                                    DEBUG_SUBP_ID("processSPU: %12d\n", SPUassembler.getPts());
                                    seenPTS(VideoPts);
                                }
                                //DEBUG_SPU("spu header: %d", spuh);
                                break;
                            }
                            //code is 0x00 (forced play)
                            /*if( spuh == 0  ) {
                                DEBUG_SPU("spu header: %d", spuh);
                            }
                            //error?
                            if( spuh == 5 ) {
                                DEBUG_SPU("spu decoding failure in player-dvd.c ? : %d", spuh);
                            }*/
                            //data is spanning packets
                            if( spuh > 5 ) {
                                SPUassembler.spuOffsetLast = spuh;
                                //DEBUG_SPU("next offset: %d", SPUassembler.spuOffsetLast);
                            }
                        }

                        if (SPUassembler.ready()) {
                            uint8_t *buffer = new uint8_t[SPUassembler.getSize()];
                            SPUassembler.Get(buffer, SPUassembler.getSize());
                            SPUdecoder->processSPU(SPUassembler.getPts(), buffer);
                            DEBUG_SUBP_ID("processSPU: %12d\n", SPUassembler.getPts());
                            seenPTS(pktpts);
                        }
                        DEBUG_SUBP_ID("packet shown spu stream: got=%d, cur=%d, SPUdecoder=%d, menu=%d, feature=%d\n",
                            thisSpuId, currentNavSubpStream, SPUdecoder!=NULL,
                            IsInMenuDomain(), dvdnav_is_domain_vts(nav));
                    } else {
                        spuInUse=false;
                        /**
                         *
                        DEBUG_SUBP_ID("packet not shown spu stream: got=%d, cur=%d, SPUdecoder=%d, menu=%d, feature=%d\n",
                            thisSpuId, currentNavSubpStream, SPUdecoder!=NULL,
                            IsInMenuDomain(), dvdnav_is_domain_vts(nav));
                         */
                    }
                }
                default:
                    DEBUG("PRIVATE_STREAM2 unhandled (a)id: %d 0x%X\n",
                        audioType, audioType);
                break;
            } /* audioType */
        } /* PRIVATE_STREAM2 */
        break;
    default:
    case SYSTEM_HEADER:
    case PROG_STREAM_MAP: {
        esyslog("ERROR: don't know what to do - packetType: %x",
        cPStream::packetType(sector));
        return;
         }
    }
    if( rframe && ringBuffer->Put(rframe) ) rframe=0;

#ifdef PTSDEBUG
    if (playMode != pmPlay)
        DEBUGPTS("SCR: %8Lx,%8Ld, %8d, %c %1d, %8d (%8d) - %8d (%8d)\n",
            scr / 300, scr % 300, mux, ptype,
            ptsFlag, VideoPts, lvpts, lapts, adiff);
#endif
}

int cDvdPlayer::Resume(int lastBlocksPlayed)
{
	uint32_t currentblock, totalblocks;
	unsigned char bufi[2048];
	int event, len;
	
	while( dvdnav_get_position( nav, &currentblock, &totalblocks) != DVDNAV_STATUS_OK ) {
		if( dvdnav_get_next_block(nav, bufi, &event, &len) != DVDNAV_STATUS_OK )
    	return -1;    			
	}    

	if( dvdnav_sector_search(nav, lastBlocksPlayed, SEEK_SET) == DVDNAV_STATUS_OK ) {
		return 1;    	
	}  
  return -1;
}

bool cDvdPlayer::Save(const char * diskStamp, int arrayIndex)
{
	unsigned int pos, len;
	int lindex = -1;
	int32_t titleNo, chapterNo;
	char stamps[10][100];
	int titles[10];
	int blocks[10];
	FILE * f;
	char s[100];
	
	if( dvdnav_current_title_info(nav, &titleNo, &chapterNo) != DVDNAV_STATUS_OK )
		return false;
        
	if( dvdnav_get_position_in_title(nav, &pos, &len) == DVDNAV_STATUS_OK ) {
		//minimum of 500.000 blocks should make sure we aint in a menu
		if( len >= 500000 ) {    
		    //read array from file
    		f=fopen("/etc/vdr/.resumedat","r");
    		if (f) {    			
    			while(fgets(s,100,f)!=NULL) {    	
    				lindex++;    	
        			stamps[lindex] = s;
        			if( fgets(s,100,f)!=NULL )
        				titles[lindex] = atoi(s);
        			if( fgets(s,100,f)!=NULL )
        				blocks[lindex] = atoi(s);
    			}    			
    			fclose(f);
    		}
    		    		
    		//write
    		f=fopen("/etc/vdr/.resumedat","w");
    		if(f) {
    			//new file
    			if( arrayIndex == -1 )
    		    	fprintf(f, "%s\n%i\n%i\n", diskStamp, titleNo, pos);    		    		    		
    			//new entry
    			if( arrayIndex == 11 ) {
    				int n;
    				//maximum of entries reached ?
    				lindex == 9 ? n=1 : n=0;
    				for( int i=n; i <= lindex; i++) {
    					fprintf(f, "%s%i\n%i\n", stamps[i], titles[i], blocks[i]);
    				}
    				fprintf(f, "%s\n%i\n%i\n", diskStamp, titleNo, pos);    				
    		 	}
   				//update existing entry
   				else {
   		    		for( int i=0; i <= lindex; i++) {
   						if(i == arrayIndex)
   							fprintf(f, "%s\n%i\n%i\n", diskStamp, titleNo, pos);
   						else
   							fprintf(f, "%s%i\n%i\n", stamps[i], titles[i], blocks[i]);
   					}
   				}
   				fclose(f);
   				return true;	
			}			
		}		
	}
  	return false;
}

void cDvdPlayer::Activate(bool On)
{
  if (On)
      Start();
  else if (active) {
     running = false;
     Cancel(3);
     active = false;
     }
}

void cDvdPlayer::Pause(void)
{
  if(!DVDActiveAndRunning()) return;
  if (playMode == pmPause || playMode == pmStill)
     Play();
  else {
     LOCK_THREAD;
     if (playMode == pmFast || (playMode == pmSlow && playDir == pdBackward))
     {
    DEBUG_NAV("%s:%d: empty\n", __FILE__, __LINE__);
        Empty();
     }
     DeviceClear();
     playMode = pmPause;
     }
}

void cDvdPlayer::Stop(void)
{
  if(!DVDActiveAndRunning()) return;

  running = false;
  usleep( 100000 ) ;  // 100ms
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
     DeviceClear();
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

      clearSeenAudioTrack();
      DEBUG_SUBP_ID("GotoTitle: -> clearSeenSubpStream\n");
      clearSeenSubpStream();

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

    if( ! IsInMenuDomain() && !IsDvdNavigationForced() )
        spuInUse=false;

    navSubpStreamSeen.Clear();
    currentNavSubpStream=-1;
    notifySeenSubpStream(-1); // no subp ..
    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: seen subp cleared\n");
        SetCurrentNavSubpStreamLocked(false);
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
    navSubpStreamSeen.Add(new IntegerListObject(navSubpStream));
    navSubpStreamSeen.Sort();
#ifdef SUBPDEBUG
            printf("cDvdPlayer::cDvdPlayer: seen new subp id: 0x%X\n", navSubpStream);
        for(int i=0; i<navSubpStreamSeen.Count(); i++)
        {
        printf("SubpStream #%d: 0x%X\n",
            i, ((IntegerListObject *)navSubpStreamSeen.Get(i))->getValue());
        }
#endif

    }
}

bool cDvdPlayer::GetCurrentNavSubpStreamLocked(void) const
{ return currentNavSubpStreamLocked; }

void cDvdPlayer::SetCurrentNavSubpStreamLocked(bool lock)
{
  currentNavSubpStreamLocked=lock;
  DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: currentNavSubpStreamLocked=%s\n",
    currentNavSubpStreamLocked?"true":"false");
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

int cDvdPlayer::GetNavSubpStreamNumber (void) const
{
    return navSubpStreamSeen.Count();
}

int cDvdPlayer::SetSubpStream(int id)
{
    static char buffer[10];
    uint16_t lang_code1;
    const char * p1 = (char *)&lang_code1;
    int logchan=-1;
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

    SetCurrentNavSubpStreamLocked(true);
    currentNavSubpStream = id;

    if( currentNavSubpStream!=-1) {
        logchan= dvdnav_get_spu_logical_stream(nav, currentNavSubpStream);
        lang_code1=dvdnav_spu_stream_to_lang( nav, logchan>=0?logchan:currentNavSubpStream );
        sprintf(buffer,"%c%c", p1[1], p1[0]);
        dvdnav_spu_language_select ( nav, buffer );
    } else {
        if( ! IsInMenuDomain() && !IsDvdNavigationForced() )
            spuInUse=false;
        sprintf(buffer,"off");
    }

    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: curNavSpu set to 0x%X, idx=%d, %s\n",
        currentNavSubpStream, i, buffer);

    return id;
}

void cDvdPlayer::NextSubpStream()
{
    static char buffer[10];
    uint16_t lang_code1;
    const char * p1 = (char *)&lang_code1;
    int logchan=-1;
    int i=navSubpStreamSeen.Count()-1;

    LOCK_THREAD;
    while ( i >= 0)
    {
    if ( currentNavSubpStream == ((IntegerListObject *)navSubpStreamSeen.Get(i))->getValue() )
        break; // found
    i--;
    }

    if( i<0 ) {
          ERROR_MESSAGE("Current subp stream not seen!");
          esyslog("ERROR: internal error current subp stream 0x%X not seen !\n",
            currentNavSubpStream);
          return;
    }

    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: found curNavSubp(0x%X) at idx = %d\n",
        currentNavSubpStream, i);

    SetCurrentNavSubpStreamLocked(true);

    //switch normal subs/ forced subs only
    if( forcedSubsOnly || currentNavSubpStream==-1 ) {
        i = ( i + 1 ) % navSubpStreamSeen.Count();
        forcedSubsOnly = false;
        SPUassembler.spuOffsetLast = 0;
    }
    else {
        i = ( i ) % navSubpStreamSeen.Count();
        forcedSubsOnly = true;
    }
    //weak end

    currentNavSubpStream = ((IntegerListObject *)navSubpStreamSeen.Get(i))->getValue();

    if( currentNavSubpStream!=-1) {
        logchan= dvdnav_get_spu_logical_stream(nav, currentNavSubpStream);
        lang_code1=dvdnav_spu_stream_to_lang( nav, logchan>=0?logchan:currentNavSubpStream );
        sprintf(buffer,"%c%c", p1[1], p1[0]);
        dvdnav_spu_language_select ( nav, buffer );
    } else {
        if( ! IsInMenuDomain() && !IsDvdNavigationForced() )
            spuInUse=false;
        sprintf(buffer,"off");
    }

    DEBUG_SUBP_ID("cDvdPlayer::cDvdPlayer: curNavSpu next to 0x%X, idx=%d, %s\n",
        currentNavSubpStream, i, buffer);

}

void cDvdPlayer::GetSubpLangCode( const char ** subplang_str ) const
{
    static char buffer[100];
    static char buff2[4];
    uint16_t lang_code1;
    int logchan=-1;
    const char * p1 = (char *)&lang_code1;

        if(!DVDActiveAndRunning()) { *subplang_str="n.a."; return; }

    if( currentNavSubpStream == -1 ) {
        strcpy(buff2,"no");
    } else {
        logchan= dvdnav_get_spu_logical_stream(nav, currentNavSubpStream);
        lang_code1=dvdnav_spu_stream_to_lang( nav, logchan>=0?logchan:currentNavSubpStream );

        if(lang_code1==0xffff)
            buff2[0]=0;
        else  {
            buff2[0]=p1[1];
            buff2[1]=p1[0];
            buff2[2]=0;
            //weak start
            if( forcedSubsOnly ) {
                buff2[0]='f';
                buff2[1]='o';
                buff2[2]='r';
                buff2[3]='c';
                buff2[4]='e';
                buff2[5]='d';
                buff2[6]='-';
                buff2[7]=p1[1];
                buff2[8]=p1[0];
            }
            //weak end
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
        SetCurrentNavAudioTrackLocked(false);
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
    navAudioTracksSeen.Add(new IntegerListObject(navAudioTrack));
    navAudioTracksSeen.Sort();
#ifdef AUDIOIDDEBUG
            printf("cDvdPlayer::cDvdPlayer: seen new audio id: 0x%X\n", navAudioTrack);
        for(int i=0; i<navAudioTracksSeen.Count(); i++)
        {
        printf("AudioTrack #%d: 0x%X\n",
            i, ((IntegerListObject *)navAudioTracksSeen.Get(i))->getValue());
        }
#endif

    }
}

bool cDvdPlayer::GetCurrentNavAudioTrackLocked(void) const
{ return currentNavAudioTrackLocked; }

void cDvdPlayer::SetCurrentNavAudioTrackLocked(bool lock)
{
  currentNavAudioTrackLocked=lock;
  DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: currentNavAudioTrackLocked=%s\n",
    currentNavAudioTrackLocked?"true":"false");
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

int  cDvdPlayer::GetCurrentNavAudioTrackType(void) const
{
    return  currentNavAudioTrackType; // aAC3, aDTS, aLPCM, aMPEG
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
    int logchan=-1;
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

    SetCurrentNavAudioTrackLocked(true);
    currentNavAudioTrack = id;

    logchan=dvdnav_get_audio_logical_stream( nav, currentNavAudioTrack );
    lang_code1=dvdnav_audio_stream_to_lang( nav, logchan>=0?logchan:currentNavAudioTrack );

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
    int logchan=-1;
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
        ERROR_MESSAGE("Current audio track not seen!");
        esyslog("ERROR: internal error current audio track 0x%X not seen !\n",
            currentNavAudioTrack);
        return;
    }

    DEBUG_AUDIO_ID("cDvdPlayer::cDvdPlayer: found curNavAu(0x%X) at idx = %d\n",
        currentNavAudioTrack, i);

    Empty();

    SetCurrentNavAudioTrackLocked(true);
    i = ( i + 1 ) % navAudioTracksSeen.Count();
    currentNavAudioTrack = ((IntegerListObject *)navAudioTracksSeen.Get(i))->getValue();

    logchan=dvdnav_get_audio_logical_stream( nav, currentNavAudioTrack );
    lang_code1=dvdnav_audio_stream_to_lang( nav, logchan>=0?logchan:currentNavAudioTrack );

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

void cDvdPlayer::GetTitleString( const char ** title_str ) const
{
    static const char * title_holder = NULL;
    static const char * dummy = "DVD Title";
        dvdnav_status_t res;

        if(!DVDActiveAndRunning()) { *title_str=dummy; return; }

    res=dvdnav_get_title_string(nav, &title_holder);
        if(res!=DVDNAV_STATUS_OK)
        title_holder=dummy;

        *title_str=title_holder;
}

void cDvdPlayer::GetTitleInfoString( const char ** title_str ) const
{
    static const char * dummy = "DVD Title";
    static char buffer[100];
    int titleNumber=-1, titleNo=-1, chapterNumber=-1, chapterNo=-1;
    int num_angle = -1, cur_angle = -1;

        if(!DVDActiveAndRunning()) { *title_str=dummy; return; }

    dvdnav_get_number_of_titles(nav, &titleNumber);
    dvdnav_current_title_info(nav, &titleNo, &chapterNo);
        dvdnav_get_number_of_parts(nav, titleNo, &chapterNumber);

    if(titleNo >= 1) {
            /* no menu here */
            /* Reflect angle info if appropriate */
            dvdnav_get_angle_info(nav, &cur_angle, &num_angle);
        }

    if(num_angle>1)
        snprintf(buffer, sizeof(buffer), "%d/%d %d/%d %d/%d",
            titleNo, titleNumber,  chapterNo, chapterNumber,
            cur_angle, num_angle);
    else
        snprintf(buffer, sizeof(buffer), "%d/%d %d/%d",
            titleNo, titleNumber,  chapterNo, chapterNumber);

        *title_str=buffer;
        return;
}

void cDvdPlayer::GetAudioLangCode( const char ** audiolang_str ) const
{
    static char buffer[100];
    static char buff2[4];
    char * audioTypeDescr = NULL;
    uint16_t lang_code1;
    int logchan=-1;
    const char * p1 = (char *)&lang_code1;

        if(!DVDActiveAndRunning()) { *audiolang_str="n.a."; return; }

    logchan=dvdnav_get_audio_logical_stream( nav, currentNavAudioTrack );
    lang_code1=dvdnav_audio_stream_to_lang( nav, logchan>=0?logchan:currentNavAudioTrack );

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

void cDvdPlayer::GetAspectString( const char ** aspect_str ) const
{
    static char buffer[100];
    int aspect;

        if(!DVDActiveAndRunning()) { *aspect_str="n.a."; return; }

    aspect=dvdnav_get_video_aspect(nav);

    switch(aspect) {
        case 0: sprintf(buffer, " 4:3"); break;
        case 2: sprintf(buffer, "16:9_"); break;
        case 3: sprintf(buffer, "16:9"); break;
        default: buffer[0]=0;
    }

    *aspect_str = buffer;
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

bool cDvdPlayer::setDiskStamp(const char ** stamp_str, dvdnav_t * nav ) const
{
    const char * titleString;
		uint32_t currentblock, totalblocks;
    unsigned char bufi[2048];
    static char buffer[100];
    int event, len;
    int titleNumber=-1, titleNo=-1, chapterNumber=-1, chapterNo=-1;

   	//open dvdnav
		if (dvdnav_open(&nav, const_cast<char *>(cDVD::getDVD()->DeviceName())) == DVDNAV_STATUS_ERR) {
        ERROR_MESSAGE("Error opening DVD!");
        return false;
    }
    dvdnav_set_readahead_flag(nav, DVDSetup.ReadAHead);
    if (DVDSetup.PlayerRCE != 0)
        dvdnav_set_region_mask(nav, 1 << (DVDSetup.PlayerRCE - 1));
    else
        dvdnav_set_region_mask(nav, 0xffff);

		//try to get infos
		if( dvdnav_get_title_string(nav, &titleString ) != DVDNAV_STATUS_OK)
			return false;
    if( dvdnav_get_number_of_titles(nav, &titleNumber) != DVDNAV_STATUS_OK)
    	return false;
    if( dvdnav_current_title_info(nav, &titleNo, &chapterNo) != DVDNAV_STATUS_OK)
    	return false;
    if( dvdnav_get_number_of_parts(nav, titleNo, &chapterNumber) != DVDNAV_STATUS_OK)
			return false;

    while( dvdnav_get_position( nav, &currentblock, &totalblocks) != DVDNAV_STATUS_OK ) {
    	if( dvdnav_get_next_block(nav, bufi, &event, &len) != DVDNAV_STATUS_OK )
    		return false;
    }
    //reset dvd_nav
    dvdnav_reset(nav);   

	snprintf(buffer, sizeof(buffer), "%s-%i-%i-%i-%i-%i", titleString, titleNumber, titleNo, chapterNumber, chapterNo, totalblocks);

	*stamp_str = buffer;
	return true;
}

void cDvdPlayer::checkDiskStamps(const char * stamp_str, int &lastTitle, int &lastBlocks, int &lastArrayIndex)
{	
	char stamps[10][100];
	int titles[10];
	int blocks[10];
	int lindex = -1;
	FILE * f;
    char s[100];
    
    //read array from file
    f=fopen("/etc/vdr/.resumedat","r");
    if (!f)
    	return;
    while(fgets(s,100,f)!=NULL) {    	
    	lindex++;    	
        stamps[lindex] = s;
        if( fgets(s,100,f)!=NULL )
        	titles[lindex] = atoi(s);
        if( fgets(s,100,f)!=NULL )
        	blocks[lindex] = atoi(s);
    }    
    fclose(f);    
		
	//check if stamp matches		
	for( int i = 0; i <= lindex; i++) {	
		//append newline char
		snprintf(s, 100, "%s\n", stamp_str);		
		if( strcmp(stamps[i], s) == 0 ) {
			lastTitle =  titles[i];
			lastBlocks = blocks[i];
			lastArrayIndex = i;
			return;
		}
	}
	lastArrayIndex = 11;
}

bool cDvdPlayer::askForResume(int blocks)
{	
#if VDRVERSNUM<10307
	OsdOpen(0, controller->osdPos-1);
#else	
	controller->OsdOpen();
#endif
		
#if VDRVERSNUM<10309		
	controller->DisplayAtBottom("Press OK to resume.");
#else
	controller->displayReplay->SetMessage(mtWarning, "Press OK to resume.");
#endif

	int count = 0;
	while( controller->getResumeValue() == 0 && count <= 40) {
		usleep(100000);
		count++;
	}		
	controller->OsdClose();	
	if( controller->getResumeValue() == 1 ) {
			controller->setResumeValue(2);
			return true;			
	}
	else {
		controller->setResumeValue(2);
		return false;
	}
}
