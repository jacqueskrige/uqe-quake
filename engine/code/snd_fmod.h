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

#ifndef _SND_FMOD_H_
#define _SND_FMOD_H_

#define UQE_FMOD

#ifdef UQE_FMOD
#define UQE_FMOD_CDAUDIO

void FMOD_Init (void);
void FMOD_Shutdown (void);
void FMOD_MusicStart (char *name, qboolean loop, qboolean notify);
void FMOD_MusicStop (void);
void FMOD_MusicPause (void);
void FMOD_MusicResume (void);
void FMOD_MusicUpdate (char *newbgmtype);
void FMOD_MusicActivate (qboolean active);
qboolean FMOD_MusicActive (void);
#endif

#endif
