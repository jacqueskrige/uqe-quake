/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
/*
** VID_GAMMA.C
*/

#include "quakedef.h"

#ifdef _WIN32
#include "winquake.h"
#endif

static unsigned char gl_gammatable[256];

static int overbrightBits;
static unsigned short oldHardwareGamma[3][256];
qboolean deviceSupportsGamma = false;
qboolean deviceSupportsGamma3DFX = false;

/*
** VG_CheckHardwareGamma
**
** Determines if the underlying hardware supports the Win32 gamma correction API.
*/
void VG_CheckHardwareGamma( void )
{
	HDC	hDC;

	deviceSupportsGamma = false;
	deviceSupportsGamma3DFX = false;

	if ( qwglSetDeviceGammaRamp3DFX )
	{
		deviceSupportsGamma3DFX = true;

		hDC = GetDC( GetDesktopWindow() );
		deviceSupportsGamma3DFX = qwglGetDeviceGammaRamp3DFX( hDC, oldHardwareGamma );
		ReleaseDC( GetDesktopWindow(), hDC );

		return;
	}

	if ( !deviceSupportsGamma3DFX )
	{
		hDC = GetDC( GetDesktopWindow() );
		deviceSupportsGamma = GetDeviceGammaRamp( hDC, oldHardwareGamma );
		ReleaseDC( GetDesktopWindow(), hDC );

		if ( deviceSupportsGamma )
		{
			// do a sanity check on the gamma values
			if ( ( HIBYTE( oldHardwareGamma[0][255] ) <= HIBYTE( oldHardwareGamma[0][0] ) ) ||
				 ( HIBYTE( oldHardwareGamma[1][255] ) <= HIBYTE( oldHardwareGamma[1][0] ) ) ||
				 ( HIBYTE( oldHardwareGamma[2][255] ) <= HIBYTE( oldHardwareGamma[2][0] ) ) )
			{
				deviceSupportsGamma = false;
				Con_Printf("Device has broken gamma support\n");
			}

			// make sure that we didn't have a prior crash in the game, and if so we need to
			// restore the gamma values to at least a linear value
			if ( ( HIBYTE( oldHardwareGamma[0][181] ) == 255 ) )
			{
				int g;

				Con_Printf("Suspicious gamma tables, using linear ramp for restoration\n");

				for ( g = 0; g < 255; g++ )
				{
					oldHardwareGamma[0][g] = g << 8;
					oldHardwareGamma[1][g] = g << 8;
					oldHardwareGamma[2][g] = g << 8;
				}
			}
		}
	}
}

/*
** VG_SetGamma
**
** This routine should only be called if deviceSupportsGamma is TRUE
*/
void VG_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
	unsigned short table[3][256];
	int		i, j;
	int		ret;
	//OSVERSIONINFO	vinfo; // jkrige - remove windows version check
	HDC hDC;

	hDC = GetDC( GetDesktopWindow() );

	if ( (!deviceSupportsGamma && !deviceSupportsGamma3DFX) || !hDC )
		return;

	for ( i = 0; i < 256; i++ ) {
		table[0][i] = ( ( ( unsigned short ) red[i] ) << 8 ) | red[i];
		table[1][i] = ( ( ( unsigned short ) green[i] ) << 8 ) | green[i];
		table[2][i] = ( ( ( unsigned short ) blue[i] ) << 8 ) | blue[i];
	}

	// jkrige - remove windows version check - begin
	// Win2K puts this odd restriction on gamma ramps...
	/*vinfo.dwOSVersionInfoSize = sizeof(vinfo);
	GetVersionEx( &vinfo );
	if ( vinfo.dwMajorVersion == 5 && vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
	{
		Con_DPrintf("performing Windows 2000 gamma clamp.\n");
		for ( j = 0 ; j < 3 ; j++ ) {
			for ( i = 0 ; i < 128 ; i++ ) {
				if ( table[j][i] > ( (128+i) << 8 ) ) {
					table[j][i] = (128+i) << 8;
				}
			}
			if ( table[j][127] > 254<<8 ) {
				table[j][127] = 254<<8;
			}
		}
	}
	else
	{
		Con_DPrintf("skipping Windows 2000 gamma clamp.\n");
	}*/
	// jkrige - remove windows version check - end

	// enforce constantly increasing
	for ( j = 0 ; j < 3 ; j++ ) {
		for ( i = 1 ; i < 256 ; i++ ) {
			if ( table[j][i] < table[j][i-1] ) {
				table[j][i] = table[j][i-1];
			}
		}
	}

	if ( qwglSetDeviceGammaRamp3DFX )
	{
		qwglSetDeviceGammaRamp3DFX( hDC, table );
	}
	else
	{
		ret = SetDeviceGammaRamp( hDC, table );
		if ( !ret )
			Con_Printf("SetDeviceGammaRamp failed.\n");
	}
	ReleaseDC( GetDesktopWindow(), hDC );
}

/*
** VG_RestoreGamma
*/
void VG_RestoreGamma( void )
{
	HDC hDC;

	hDC = GetDC( GetDesktopWindow() );

	if ( deviceSupportsGamma )
	{
		if ( qwglSetDeviceGammaRamp3DFX )
		{
			qwglSetDeviceGammaRamp3DFX( hDC, oldHardwareGamma );
		}
		else
		{
			SetDeviceGammaRamp( hDC, oldHardwareGamma );
		}
	}

	ReleaseDC( GetDesktopWindow(), hDC );
}

/*
===============
VG_GammaCorrect
===============
*/
void VG_GammaCorrect( byte *buffer, int bufSize ) {
	int i;

	for ( i = 0; i < bufSize; i++ ) {
		buffer[i] = gl_gammatable[buffer[i]];
	}
}

/*
===============
VG_SetColorMappings
===============
*/
void VG_SetColorMappings( void )
{
	int		i, j;
	float	g;
	int		inf;
	int		shift;

	// setup the overbright lighting
	overbrightBits = (int)gl_overbrightbits.value;

	if ( !deviceSupportsGamma )
		overbrightBits = 0;		// need hardware gamma for overbright

	// never overbright in windowed mode
	if ( modestate != MS_FULLSCREEN && modestate != MS_FULLDIB )
		overbrightBits = 0;

	// allow 2 overbright bits in 24 bit, but only 1 in 16 bit

	// jkrige - no 16bit mode
	//if ( vid.bpp > 16 )
	//{
		if ( overbrightBits > 2 )
			overbrightBits = 2;
	//}
	//else
	//{
	//	if (  overbrightBits > 1 )
	//		overbrightBits = 1;
	//}
	// jkrige - no 16bit mode


	if ( overbrightBits < 0 )
		overbrightBits = 0;

	if ( gl_gamma.value < 0.5f )
	{
		Cvar_SetValue ("gamma", 0.5f);
		return;
	}
	else if ( gl_gamma.value > 2.75f )
	{
		Cvar_SetValue ("gamma", 2.75f);
		return;
	}

	g = 3.0f - gl_gamma.value;

	// jkrige - quake 3
	//if ( r_intensity->value <= 1 ) {
	//	ri.Cvar_Set( "r_intensity", "1" );
	//}
	// jkrige - quake 3

	if ( gl_overbrightbits.value < 0.0f )
	{
		Cvar_SetValue ("gl_overbrightbits", 0.0f);
		return;
	}
	else if ( gl_overbrightbits.value > (float)overbrightBits )
	{
		Cvar_SetValue ("gl_overbrightbits", (float)overbrightBits);
		return;
	}

	shift = (int)gl_overbrightbits.value;

	for ( i = 0; i < 256; i++ ) {
		if ( g == 1 ) {
			inf = i;
		} else {
			inf = 255 * pow ( i/255.0f, 1.0f / g ) + 0.5f;
		}
		inf <<= shift;
		if (inf < 0) {
			inf = 0;
		}
		if (inf > 255) {
			inf = 255;
		}
		gl_gammatable[i] = inf;
	}

	// jkrige - quake 3
	/*for (i=0 ; i<256 ; i++) {
		j = i * r_intensity->value;
		if (j > 255) {
			j = 255;
		}
		s_intensitytable[i] = j;
	}*/
	// jkrige - quake 3

	if ( deviceSupportsGamma )
	{
		VG_SetGamma( gl_gammatable, gl_gammatable, gl_gammatable );
	}
}