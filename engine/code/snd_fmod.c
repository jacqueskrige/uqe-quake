/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// snd_fmod.c: FMOD sound system implementation for music playback

// Developed by Jacques Krige
// Ultimate Quake Engine
// http://www.jacqueskrige.com


#include "quakedef.h"


#ifdef _WIN32
#include "winquake.h"
#endif


#ifndef UQE_FMOD_CDAUDIO
#include "cdaudio.h"
#endif

#ifdef UQE_FMOD

#pragma comment(lib, "../fmod-4/lib/fmodex_vc.lib")

#include "../fmod-4/inc/fmod.h"
#include "../fmod-4/inc/fmod_errors.h"

extern int sound_started;

typedef struct
{
	void			*data;
	int				length;
	char			filename[MAX_QPATH];
} SND_File_t;

typedef struct
{
	float			volume;
	FMOD_CHANNEL	*channel;
	int				track;
	char			trackname[MAX_QPATH];
	qboolean		inuse;
	qboolean		looping;
	int				loopcount;
	qboolean		paused;
} SND_Channel_t;


SND_File_t		SND_File;
SND_Channel_t	SND_MusicChannel;

FMOD_SYSTEM		*fmod_system;
FMOD_SOUND		*fmod_sound;
FMOD_SOUND		*fmod_compactdisc;

FMOD_RESULT		fmod_result;

qboolean		SND_Initialised;
qboolean		SND_InitialisedCD;

int				oldtrack;

char			bgmtype[16];
char			oldbgmtype[16];

float			oldbgmvolume;


// forward declarations
void FMOD_Restart (void);
void FMOD_MusicStartConsole (void);
void FMOD_MusicUpdate(char *newbgmtype);



// ===================================================================================
//
//  CUSTOM FMOD FILE ROUTINES TO ALLOW PAK FILE ACCESS
//
// ===================================================================================

/*
===================
SND_FOpen
===================
*/
qboolean SND_FOpen (const char *name)
{
	int		len;
	FILE	*f;

	if ((len = COM_FOpenFile((char *)name, &f)) < 1)
	{
		Con_Printf("SND_FOpen: Failed to open %s, file not found\n", name);
		return false;
	}

	if (!SND_File.length)
	{
		strcpy(SND_File.filename, name);
		SND_File.length = len;
		SND_File.data = COM_FReadFile(f, len);
		Con_DPrintf("SND_FOpen: Sucessfully opened %s\n", name);

		return true;
	}

	if (f)
		fclose(f);

	f = NULL;

	Con_SafePrintf("SND_FOpen: Failed to open %s, insufficient handles\n", name);

	return false;
}

/*
===================
SND_FClose
===================
*/
void SND_FClose (void)
{
	if (!SND_File.data)
		return;

	SND_File.length = 0;
	strcpy(SND_File.filename, "\0");

	if (SND_File.data)
		free(SND_File.data);

	SND_File.data = NULL;
}



// ===================================================================================
//
//  STARTUP AND SHUTDOWN ROUTINES
//
// ===================================================================================

/*
===================
FMOD_ERROR
===================
*/
void FMOD_ERROR(FMOD_RESULT result, qboolean notify, qboolean syserror)
{
	if (result != FMOD_OK)
	{
		if (syserror == false)
		{
			if (notify == true)
				Con_Printf("%s\n", FMOD_ErrorString(result));
		}
		else
			Sys_Error("FMOD: %s\n", FMOD_ErrorString(result));
	}
}

/*
===================
CDA_Startup
===================
*/
void CDA_Startup (qboolean notify)
{
#ifdef UQE_FMOD_CDAUDIO
	int i;
	int numdrives;
	int	numtracks;

	if (SND_InitialisedCD == true)
		return;

	if (fmod_compactdisc)
		return;

	// bump up the file buffer size a bit from the 16k default for CDDA, because it is a slower medium.
	fmod_result = FMOD_System_SetStreamBufferSize(fmod_system, 64*1024, FMOD_TIMEUNIT_RAWBYTES);
    FMOD_ERROR(fmod_result, notify, false);

	fmod_result = FMOD_System_GetNumCDROMDrives(fmod_system, &numdrives);
	FMOD_ERROR(fmod_result, notify, false);

	for (i = 0; i < numdrives; i++)
	{
		char drivename[MAX_QPATH];
		char scsiname[MAX_QPATH];
		char devicename[MAX_QPATH];

		fmod_result = FMOD_System_GetCDROMDriveName(fmod_system, i, drivename, MAX_QPATH, scsiname, MAX_QPATH, devicename, MAX_QPATH);
		FMOD_ERROR(fmod_result, notify, false);

		if (fmod_result == FMOD_OK)
		{
			fmod_result = FMOD_System_CreateStream(fmod_system, drivename, FMOD_OPENONLY, 0, &fmod_compactdisc);

			if (fmod_result == FMOD_OK)
			{
				fmod_result = FMOD_Sound_GetNumSubSounds(fmod_compactdisc, &numtracks);
				FMOD_ERROR(fmod_result, notify, false);

				if (fmod_result == FMOD_OK)
				{
					Con_Printf("CD Audio Initialized (%s)\n", drivename);
					SND_InitialisedCD = true;

					break;
				}
			}
		}
	}

	FMOD_ERROR(fmod_result, notify, false);
#else
	CDAudio_Init();
#endif
}

/*
===================
CDA_Shutdown
===================
*/
void CDA_Shutdown (void)
{
#ifdef UQE_FMOD_CDAUDIO
	if (SND_InitialisedCD == false)
		return;

	if (fmod_compactdisc)
	{
		fmod_result = FMOD_Sound_Release(fmod_compactdisc);
		FMOD_ERROR(fmod_result, true, false);

		fmod_compactdisc = NULL;
	}

	SND_InitialisedCD = false;
#else
	CDAudio_Shutdown ();
#endif
}

/*
===================
FMOD_Startup
===================
*/
void FMOD_Startup (void)
{
	FMOD_CAPS			caps;
	FMOD_SPEAKERMODE	speakermode;
	FMOD_OUTPUTTYPE		fmod_output;
	unsigned int		version;
	int					numdrivers;
	char				name[256];
	int					SND_SoftwareChannels;
	int					SND_HardwareChannels;
	int					SND_Bits;
	int					SND_Rate;

	fmod_result = FMOD_System_Create(&fmod_system);
    FMOD_ERROR(fmod_result, true, false);

	fmod_result = FMOD_System_GetVersion(fmod_system, &version);
    FMOD_ERROR(fmod_result, true, false);

    if (version < FMOD_VERSION)
    {
		Con_Printf("\nFMOD version incorrect, found v%1.2f, requires v%1.2f or newer\n", version, FMOD_VERSION);
        return;
    }

	fmod_result = FMOD_System_GetNumDrivers(fmod_system, &numdrivers);
	FMOD_ERROR(fmod_result, true, false);

	if (numdrivers == 0)
	{
		fmod_result = FMOD_System_SetOutput(fmod_system, FMOD_OUTPUTTYPE_NOSOUND);
		FMOD_ERROR(fmod_result, true, false);
	}
	else
	{
		fmod_result = FMOD_System_SetOutput(fmod_system, FMOD_OUTPUTTYPE_AUTODETECT);
		FMOD_ERROR(fmod_result, true, false);

		fmod_result = FMOD_System_GetDriverCaps(fmod_system, 0, &caps, NULL, &speakermode);
		FMOD_ERROR(fmod_result, true, false);

		// set the user selected speaker mode
		fmod_result = FMOD_System_SetSpeakerMode(fmod_system, FMOD_SPEAKERMODE_STEREO /*speakermode*/);
		FMOD_ERROR(fmod_result, true, false);

		if (caps & FMOD_CAPS_HARDWARE_EMULATED)
		{
			// the user has the 'Acceleration' slider set to off. this is really bad for latency!
			fmod_result = FMOD_System_SetDSPBufferSize(fmod_system, 1024, 10);
			FMOD_ERROR(fmod_result, true, false);

			Con_Printf("\nHardware Acceleration is turned off!\n");
		}

		fmod_result = FMOD_System_GetDriverInfo(fmod_system, 0, name, 256, NULL);
		FMOD_ERROR(fmod_result, true, false);
		
		if (strstr(name, "SigmaTel"))
		{
			// Sigmatel sound devices crackle for some reason if the format is PCM 16bit.
			// PCM floating point output seems to solve it.
			fmod_result = FMOD_System_SetSoftwareFormat(fmod_system, 48000, FMOD_SOUND_FORMAT_PCMFLOAT, 0, 0, FMOD_DSP_RESAMPLER_LINEAR);
			FMOD_ERROR(fmod_result, true, false);
		}
	}

	fmod_result = FMOD_System_GetSoftwareChannels(fmod_system, &SND_SoftwareChannels);
	FMOD_ERROR(fmod_result, true, false);

	fmod_result = FMOD_System_GetSoftwareFormat(fmod_system, &SND_Rate, NULL, NULL, NULL, NULL, &SND_Bits);
	FMOD_ERROR(fmod_result, true, false);

	fmod_result = FMOD_System_Init(fmod_system, MAX_CHANNELS, FMOD_INIT_NORMAL, NULL);
    FMOD_ERROR(fmod_result, true, false);

	if (fmod_result == FMOD_ERR_OUTPUT_CREATEBUFFER)
	{
		// the speaker mode selected isn't supported by this soundcard. Switch it back to stereo...
		fmod_result = FMOD_System_SetSpeakerMode(fmod_system, FMOD_SPEAKERMODE_STEREO);
		FMOD_ERROR(fmod_result, true, false);

		// ... and re-init.
		fmod_result = FMOD_System_Init(fmod_system, MAX_CHANNELS, FMOD_INIT_NORMAL, NULL);
		FMOD_ERROR(fmod_result, true, false);
	}

	fmod_result = FMOD_System_GetSpeakerMode(fmod_system, &speakermode);
	FMOD_ERROR(fmod_result, true, false);

	fmod_result = FMOD_System_GetOutput(fmod_system, &fmod_output);
	FMOD_ERROR(fmod_result, true, false);

	fmod_result = FMOD_System_GetHardwareChannels(fmod_system, &SND_HardwareChannels);
	FMOD_ERROR(fmod_result, true, false);


	// print all the sound information to the console
	Con_Printf("\nFMOD version %01x.%02x.%02x\n", (FMOD_VERSION >> 16) & 0xff, (FMOD_VERSION >> 8) & 0xff, FMOD_VERSION & 0xff);

	switch (fmod_output)
	{
		case FMOD_OUTPUTTYPE_NOSOUND:
			Con_Printf("using No Sound\n");
			break;

		case FMOD_OUTPUTTYPE_DSOUND:
			Con_Printf("using Microsoft DirectSound\n");
			break;

		case FMOD_OUTPUTTYPE_WINMM:
			Con_Printf("using Windows Multimedia\n");
			break;

		case FMOD_OUTPUTTYPE_WASAPI:
			Con_Printf("using Windows Audio Session API\n");
			break;

		case FMOD_OUTPUTTYPE_ASIO:
			Con_Printf("using Low latency ASIO\n");
			break;
	}
	
	Con_Printf("   software channels: %i\n", SND_SoftwareChannels);
	Con_Printf("   hardware channels: %i\n", SND_HardwareChannels);
	Con_Printf("   %i bits/sample\n", SND_Bits);
	Con_Printf("   %i bytes/sec\n", SND_Rate);

	switch (speakermode)
	{
		case FMOD_SPEAKERMODE_RAW:
			Con_Printf("Speaker Output: Raw\n");
			break;

		case FMOD_SPEAKERMODE_MONO:
			Con_Printf("Speaker Output: Mono\n");
			break;

		case FMOD_SPEAKERMODE_STEREO:
			Con_Printf("Speaker Output: Stereo\n");
			break;

		case FMOD_SPEAKERMODE_QUAD:
			Con_Printf("Speaker Output: Quad\n");
			break;

		case FMOD_SPEAKERMODE_SURROUND:
			Con_Printf("Speaker Output: Surround\n");
			break;

		case FMOD_SPEAKERMODE_5POINT1:
			Con_Printf("Speaker Output: 5.1\n");
			break;

		case FMOD_SPEAKERMODE_7POINT1:
			Con_Printf("Speaker Output: 7.1\n");
			break;

		case FMOD_SPEAKERMODE_SRS5_1_MATRIX:
			Con_Printf("Speaker Output: Stereo compatible\n");
			break;

		case FMOD_SPEAKERMODE_MYEARS:
			Con_Printf("Speaker Output: Headphones\n");
			break;

		default:
			Con_Printf("Speaker Output: Unknown\n");
	}

	strcpy(bgmtype, "cd");
	oldbgmvolume = bgmvolume.value;

	SND_FClose();
	CDA_Startup(true);

	SND_Initialised = true;

	// clear music channel
	SND_MusicChannel.volume = 0.0f;
	SND_MusicChannel.channel = NULL;
	SND_MusicChannel.track = 0;
	strcpy(SND_MusicChannel.trackname, "\0");
	SND_MusicChannel.inuse = false;
	SND_MusicChannel.looping = false;
	SND_MusicChannel.loopcount = 0;
	SND_MusicChannel.paused = false;
}

/*
===================
FMOD_Shutdown
===================
*/
void FMOD_Shutdown(void)
{
	if (COM_CheckParm("-nosound"))
	{
		SND_Initialised = false;
		SND_InitialisedCD = false;
		return;
	}

	FMOD_MusicStop();
	CDA_Shutdown();

	if (fmod_system)
	{
		fmod_result = FMOD_System_Close(fmod_system);
		FMOD_ERROR(fmod_result, true, false);

		fmod_result = FMOD_System_Release(fmod_system);
		FMOD_ERROR(fmod_result, true, false);

		fmod_system = NULL;
	}

	SND_Initialised = false;

	// clear music channel
	SND_MusicChannel.volume = 0.0f;
	SND_MusicChannel.channel = NULL;
	SND_MusicChannel.track = 0;
	strcpy(SND_MusicChannel.trackname, "\0");
	SND_MusicChannel.inuse = false;
	SND_MusicChannel.looping = false;
	SND_MusicChannel.loopcount = 0;
	SND_MusicChannel.paused = false;
}

/*
===================
FMOD_Init
===================
*/
void FMOD_Init (void)
{
	SND_Initialised = false;
	SND_InitialisedCD = false;

	if (!sound_started)
		return;

	if (COM_CheckParm("-nosound"))
		return;

	FMOD_Startup();

	Cmd_AddCommand("fmod_restart", FMOD_Restart);
	Cmd_AddCommand("fmod_playmusic", FMOD_MusicStartConsole);
	Cmd_AddCommand("fmod_stopmusic", FMOD_MusicStop);
	Cmd_AddCommand("fmod_pausemusic", FMOD_MusicPause);
	Cmd_AddCommand("fmod_resumemusic", FMOD_MusicResume);

	//Con_Printf("--------------------------------------\n");
}

/*
===================
FMOD_Restart
===================
*/
void FMOD_Restart (void)
{
	int current_track;
	char current_trackname[MAX_QPATH];
	qboolean current_inuse;
	qboolean current_looping;

	if (!sound_started)
		return;

	if (COM_CheckParm("-nosound"))
	{
		SND_Initialised = false;
		SND_InitialisedCD = false;
		return;
	}

	current_inuse = SND_MusicChannel.inuse;

	// save music info
	if (SND_MusicChannel.inuse == true)
	{
		current_track = SND_MusicChannel.track;
		strcpy(current_trackname, SND_MusicChannel.trackname);
		current_looping = SND_MusicChannel.looping;
		FMOD_MusicStop();
	}
	else
	{
		current_track = 0;
		strcpy(current_trackname, "\0");
		current_looping = false;
	}

	FMOD_Shutdown();
	FMOD_Startup();

	// restart music if needed
	if (current_inuse == true)
	{
		if(current_track > 0)
			FMOD_MusicStart(va("%i", (int)current_track), current_looping, false);
		else
			FMOD_MusicStart(current_trackname, current_looping, false);
	}
}

/*
===================
FMOD_ChannelStart
===================
*/
void FMOD_ChannelStart (FMOD_SOUND *sound, qboolean loop, qboolean paused)
{
	fmod_result = FMOD_System_PlaySound(fmod_system, FMOD_CHANNEL_FREE, sound, (FMOD_BOOL)paused, &SND_MusicChannel.channel);
	FMOD_ERROR(fmod_result, true, false);

	if ((SND_MusicChannel.looping = loop) == true)
	{
		fmod_result = FMOD_Channel_SetMode(SND_MusicChannel.channel, FMOD_LOOP_NORMAL);
		FMOD_ERROR(fmod_result, true, false);
	}
	else
	{
		fmod_result = FMOD_Channel_SetMode(SND_MusicChannel.channel, FMOD_LOOP_OFF);
		FMOD_ERROR(fmod_result, true, false);

		fmod_result = FMOD_Channel_SetLoopCount(SND_MusicChannel.channel, 0);
		FMOD_ERROR(fmod_result, true, false);
	}

	SND_MusicChannel.inuse = true;
}



// ===================================================================================
//
//  CD AUDIO CONTROL ROUTINES
//
// ===================================================================================

/*
===================
CDA_Start
===================
*/
void CDA_Start(int track, qboolean loop, qboolean notify)
{
#ifdef UQE_FMOD_CDAUDIO
	int numtracks;

	if (SND_InitialisedCD == false)
		return;

	if (SND_MusicChannel.inuse == true)
		FMOD_MusicStop();

	if (track <= 0)
		return;

	fmod_result = FMOD_Sound_GetNumSubSounds(fmod_compactdisc, &numtracks);
	FMOD_ERROR(fmod_result, notify, false);

	if (track > numtracks)
		return;

	// fmod track numbers starts at zero
	fmod_result = FMOD_Sound_GetSubSound(fmod_compactdisc, track - 1, &fmod_sound);
	FMOD_ERROR(fmod_result, notify, false);

	if (fmod_result == FMOD_OK)
		FMOD_ChannelStart(fmod_sound, loop, false);
#else
	CDAudio_Play(track, loop);
#endif
}

/*
===================
CDA_Stop
===================
*/
void CDA_Stop (void)
{
#ifdef UQE_FMOD_CDAUDIO
	if (SND_InitialisedCD == false)
		return;

	if (SND_MusicChannel.inuse == false)
		return;

	if (SND_MusicChannel.channel)
	{
		fmod_result = FMOD_Channel_Stop(SND_MusicChannel.channel);
		FMOD_ERROR(fmod_result, true, false);
	}

	SND_MusicChannel.inuse = false;
	SND_MusicChannel.looping = false;
	SND_MusicChannel.loopcount = 0;
	SND_MusicChannel.paused = false;
#else
	CDAudio_Stop();
#endif
}

/*
===================
CDA_Pause
===================
*/
void CDA_Pause (void)
{
#ifdef UQE_FMOD_CDAUDIO
	if (SND_InitialisedCD == false)
		return;

	if (SND_MusicChannel.inuse == false)
		return;

	if (SND_MusicChannel.paused == false)
	{
		fmod_result = FMOD_Channel_SetPaused(SND_MusicChannel.channel, true);
		FMOD_ERROR(fmod_result, true, false);

		SND_MusicChannel.paused = true;
	}
#else
	CDAudio_Pause();
#endif
}

/*
===================
CDA_Resume
===================
*/
void CDA_Resume (qboolean force)
{
#ifdef UQE_FMOD_CDAUDIO
	if (SND_InitialisedCD == false)
		return;

	if (SND_MusicChannel.inuse == false)
		return;

	if (SND_MusicChannel.paused == true && SND_MusicChannel.volume != 0.0f && (oldbgmvolume == 0.0f | force == true))
	{
		fmod_result = FMOD_Channel_SetPaused(SND_MusicChannel.channel, false);
		FMOD_ERROR(fmod_result, true, false);

		SND_MusicChannel.paused = false;
	}
#else
	CDAudio_Resume();
#endif
}

/*
===================
CDA_Update
===================
*/
void CDA_Update (void)
{
#ifdef UQE_FMOD_CDAUDIO
	if (SND_Initialised == false || SND_MusicChannel.inuse == false)
		return;

	FMOD_System_Update(fmod_system);
	SND_MusicChannel.volume = bgmvolume.value;

	if (SND_MusicChannel.volume < 0.0f)
		SND_MusicChannel.volume = 0.0f;

	if (SND_MusicChannel.volume > 1.0f)
		SND_MusicChannel.volume = 1.0f;

	FMOD_Channel_SetVolume(SND_MusicChannel.channel, SND_MusicChannel.volume);

	if (SND_MusicChannel.volume == 0.0f)
		CDA_Pause();
	else
		CDA_Resume(false);
#else
	CDAudio_Update();
#endif
}



// ===================================================================================
//
//  MOD AUDIO CONTROL ROUTINES (OGG / MP3 / WAV)
//
// ===================================================================================

/*
===================
MOD_Start
===================
*/
void MOD_Start (char *name, qboolean loop, qboolean notify)
{
	char					file[MAX_QPATH];
	FMOD_CREATESOUNDEXINFO	exinfo;

	if (SND_Initialised == false)
		return;

	if (SND_MusicChannel.inuse == true)
		FMOD_MusicStop();

	if (strlen(name) == 0)
		return;

	if (SND_FOpen(name) == true)
	{
		memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
		exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
		exinfo.length = SND_File.length;

		fmod_result = FMOD_System_CreateSound(fmod_system, (const char *)SND_File.data, FMOD_HARDWARE | FMOD_OPENMEMORY | FMOD_2D, &exinfo, &fmod_sound);
		FMOD_ERROR(fmod_result, true, false);
		
		strcpy(file, SND_File.filename);
		SND_FClose();
	}
	
	if (!fmod_sound)
	{
		Con_Printf("Couldn't open stream %s\n", file);
		return;
	}
	else
	{
		if (notify == true)
			Con_Printf("Playing: %s...\n", file);
	}

	if (fmod_result == FMOD_OK)
		FMOD_ChannelStart(fmod_sound, loop, false);
}

/*
===================
MOD_Stop
===================
*/
void MOD_Stop (void)
{
	if (SND_Initialised == false || SND_MusicChannel.inuse == false)
		return;

	if (SND_MusicChannel.channel)
	{
		fmod_result = FMOD_Channel_Stop(SND_MusicChannel.channel);
		FMOD_ERROR(fmod_result, true, false);
	}

	if (fmod_sound)
	{
		fmod_result = FMOD_Sound_Release(fmod_sound);
		FMOD_ERROR(fmod_result, true, false);

		fmod_sound = NULL;
	}

	SND_MusicChannel.inuse = false;
	SND_MusicChannel.looping = false;
	SND_MusicChannel.loopcount = 0;
	SND_MusicChannel.paused = false;
}

/*
===================
MOD_Pause
===================
*/
void MOD_Pause (void)
{
	if (SND_Initialised == false || SND_MusicChannel.inuse == false)
		return;

	if (SND_MusicChannel.paused == false)
	{
		fmod_result = FMOD_Channel_SetPaused(SND_MusicChannel.channel, true);
		FMOD_ERROR(fmod_result, true, false);

		SND_MusicChannel.paused = true;
	}

	if (SND_MusicChannel.volume == 0.0f)
		SND_MusicChannel.paused = true;
}

/*
===================
MOD_Resume
===================
*/
void MOD_Resume (qboolean force)
{
	if (SND_Initialised == false || SND_MusicChannel.inuse == false)
		return;

	if (SND_MusicChannel.paused == true && SND_MusicChannel.volume != 0.0f && (oldbgmvolume == 0.0f | force == true))
	{
		fmod_result = FMOD_Channel_SetPaused(SND_MusicChannel.channel, false);
		FMOD_ERROR(fmod_result, true, false);

		SND_MusicChannel.paused = false;
	}
}

/*
===================
MOD_Update
===================
*/
void MOD_Update (void)
{
	if (SND_Initialised == false || SND_MusicChannel.inuse == false)
		return;

	FMOD_System_Update(fmod_system);
	SND_MusicChannel.volume = bgmvolume.value;

	if (SND_MusicChannel.volume < 0.0f)
		SND_MusicChannel.volume = 0.0f;

	if (SND_MusicChannel.volume > 1.0f)
		SND_MusicChannel.volume = 1.0f;

	FMOD_Channel_SetVolume(SND_MusicChannel.channel, SND_MusicChannel.volume);

	if (SND_MusicChannel.volume == 0.0f)
		MOD_Pause();
	else
		MOD_Resume(false);
}



// ===================================================================================
//
//  MAIN FMOD AUDIO CONTROL ROUTINES
//
// ===================================================================================

/*
===================
FMOD_MusicStart
===================
*/
void FMOD_MusicStart (char *name, qboolean loop, qboolean notify)
{
	int		len;
	char	file[MAX_QPATH];
	char	trackname[MAX_QPATH];
	FILE	*f;

	if (!sound_started)
		return;

	len = -1;

	// check for an existing audio file
	if (name != NULL && strlen(name) != 0)
	{
		COM_StripExtension(name, file);

		sprintf(trackname, "music/%s.ogg", file);
		len = COM_FOpenFile((char *)trackname, &f);
		if (len < 0)
		{
			sprintf(trackname, "music/%s.mp3", file);
			len = COM_FOpenFile((char *)trackname, &f);
		}
		if (len < 0)
		{
			sprintf(trackname, "music/%s.wav", file);
			len = COM_FOpenFile((char *)trackname, &f);
		}
		if (len < 0)
		{
			sprintf(trackname, "music/track%02i.ogg", atoi(file));
			len = COM_FOpenFile((char *)trackname, &f);
		}
		if (len < 0)
		{
			sprintf(trackname, "music/track%02i.mp3", atoi(file));
			len = COM_FOpenFile((char *)trackname, &f);
		}
		if (len < 0)
		{
			sprintf(trackname, "music/track%02i.wav", atoi(file));
			len = COM_FOpenFile((char *)trackname, &f);
		}

		if (f)
			fclose(f);

		f = NULL;
	}

	// set the new bgmtype
	if (len > 0)
	{
		// if an audio file exists play it
		SND_MusicChannel.track = atoi(file);
		strcpy(SND_MusicChannel.trackname, trackname);
		FMOD_MusicUpdate("mod");
	}
	else
	{
		// otherwise fall back to CD Audio
		SND_MusicChannel.track = atoi(name);
		strcpy(SND_MusicChannel.trackname, "\0");
		FMOD_MusicUpdate("cd");
	}

	if (strcmpi(bgmtype, "cd") == 0)
		CDA_Start(SND_MusicChannel.track, loop, notify);

	if (strcmpi(bgmtype, "mod") == 0)
		MOD_Start(SND_MusicChannel.trackname, loop, notify);
}

/*
===================
FMOD_MusicStartConsole
===================
*/
void FMOD_MusicStartConsole (void)
{
	char name[MAX_QPATH];
	char file[MAX_QPATH];

	if (!sound_started)
		return;

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("fmod_playmusic <filename> : play an audio file\n");
		return;
	}

	sprintf(name, "%s\0", Cmd_Argv(1));
	COM_StripExtension(name, file);

	MOD_Start(file, false, true);

	return;
}

/*
===================
FMOD_MusicStop
===================
*/
void FMOD_MusicStop (void)
{
	CDA_Stop();
	MOD_Stop();
}

/*
===================
FMOD_MusicPause
===================
*/
void FMOD_MusicPause (void)
{
	if (strcmpi(bgmtype, "cd") == 0)
		CDA_Pause();

	if (strcmpi(bgmtype, "mod") == 0)
		MOD_Pause();
}

/*
===================
FMOD_MusicResume
===================
*/
void FMOD_MusicResume (void)
{
	if (strcmpi(bgmtype, "cd") == 0)
		CDA_Resume(true);

	if (strcmpi(bgmtype, "mod") == 0)
		MOD_Resume(true);
}

/*
===================
FMOD_MusicUpdate
===================
*/
void FMOD_MusicUpdate (char *newbgmtype)
{
	if (SND_MusicChannel.track != oldtrack)
	{
		if (strcmpi(bgmtype, "cd") == 0)
			CDA_Stop();

		if (strcmpi(bgmtype, "mod") == 0)
			MOD_Stop();

		oldtrack = SND_MusicChannel.track;
	}

	if (strcmpi(bgmtype, "cd") == 0)
		CDA_Update();

	if (strcmpi(bgmtype, "mod") == 0)
		MOD_Update();

	if (newbgmtype != NULL)
		strcpy(bgmtype, newbgmtype);

	if (strcmpi(bgmtype, oldbgmtype) != 0)
	{
		FMOD_MusicStop();
		strcpy(oldbgmtype, bgmtype);
	}

	oldbgmvolume = SND_MusicChannel.volume;
}

/*
===================
FMOD_MusicActivate
===================
*/
void FMOD_MusicActivate (qboolean active)
{
	if (active)
		FMOD_MusicResume();
	else
		FMOD_MusicPause();
}

/*
===================
FMOD_MusicActive
===================
*/
qboolean FMOD_MusicActive (void)
{
	return SND_MusicChannel.inuse;
}

#endif
