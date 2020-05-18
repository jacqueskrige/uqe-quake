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

// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"

// jkrige - scale2d
#ifdef _WIN32
#include "winquake.h"
#endif
// jkrige - scale2d

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net 
turn off messages option

the refresh is allways rendered, unless the console is full screen


console is:
	notify lines
	half
	full
	

*/


int			glx, gly, glwidth, glheight;

// only the refresh window will be updated unless these variables are flagged 
int			scr_copytop;
int			scr_copyeverything;

float		scr_con_current;
float		scr_conlines;		// lines of console to display


// jkrige - scr_viewsize & statusbar
float		oldfov;
//float		oldscreensize, oldfov;
// jkrige - scr_viewsize & statusbar


cvar_t		scr_viewsize = {"viewsize","100", true};


// jkrige - field of view (fov) fix
//cvar_t		scr_fov = {"fov","90"};	// 10 - 170
// jkrige - field of view (fov) fix

// jkrige - console speed increased
//cvar_t		scr_conspeed = {"scr_conspeed","300"};
cvar_t		scr_conspeed = {"scr_conspeed","900"};
// jkrige - console speed increased


cvar_t		scr_centertime = {"scr_centertime","2"};
cvar_t		scr_showram = {"showram","1"};
cvar_t		scr_showturtle = {"showturtle","0"};
cvar_t		scr_showpause = {"showpause","1"};
cvar_t		scr_printspeed = {"scr_printspeed","8"};
cvar_t		gl_triplebuffer = {"gl_triplebuffer", "1", true };

extern	cvar_t	crosshair;

qboolean	scr_initialized;		// ready to draw

qpic_t		*scr_ram;
qpic_t		*scr_net;
qpic_t		*scr_turtle;

int			scr_fullupdate;

int			clearconsole;
int			clearnotify;

int			sb_lines;

viddef_t	vid;				// global video state

vrect_t		scr_vrect;

qboolean	scr_disabled_for_loading;
qboolean	scr_drawloading;
float		scr_disabled_time;

qboolean	block_drawing;

void SCR_ScreenShot_f (void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	strncpy (scr_centerstring, str, sizeof(scr_centerstring)-1);
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}


void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height*0.35;
	else
		y = 48;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
		{
			Draw_Character (x, y, start[j]);	
			if (!remaining--)
				return;
		}
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;
	
	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	float   a;
	float   x;

    if (fov_x < 1 || fov_x > 179)
		Sys_Error ("Bad fov: %f", fov_x);

    x = width/tan(fov_x/360*M_PI);

    a = atan (height/x);
    a = a*360/M_PI;

    return a;
}

// jkrige - field of view (fov) fix
float fov_width;
// jkrige - field of view (fov) fix

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef (void)
{
	vrect_t		vrect;
	//float		size; // jkrige - scr_viewsize & statusbar
	int		h;

	// jkrige - field of view (fov) fix
	float fov_monitor;
	// jkrige - field of view (fov) fix
	

	scr_fullupdate = 0;		// force a background redraw
	vid.recalc_refdef = 0;

	// jkrige - always draw sbar
// force the status bar to redraw
	//Sbar_Changed ();
	// jkrige - always draw sbar

//========================================
	
// bound viewsize
	// jkrige - scr_viewsize & statusbar
	if (scr_viewsize.value < 100)
		Cvar_Set ("viewsize","100"); // jkrige : was 30
	if (scr_viewsize.value > 110)
		Cvar_Set ("viewsize","110");
	// jkrige - scr_viewsize & statusbar


// bound field of view
	// jkrige - field of view (fov) fix
	/*if (scr_fov.value < 10)
		Cvar_Set ("fov","10");
	if (scr_fov.value > 170)
		Cvar_Set ("fov","170");*/
	if (fov_width < 10)
		fov_width = 10;
	if (fov_width > 170)
		fov_width = 170;
	// jkrige - field of view (fov) fix


// intermission is always full screen
	//if (cl.intermission)
	//	size = 100;
	//else
	//	size = scr_viewsize.value;


	// jkrige - viewsize & statusbar
	if(scr_viewsize.value >= 110)
		sb_lines = 24; // no inventory
	else
		sb_lines = 24 + 16 + 8;
	
	//if (size >= 110)
	//	sb_lines = 0;		// no status bar at all
	//else if (size >= 110)
	//	sb_lines = 24;		// no inventory
	//else
	//	sb_lines = 24 + 16 + 8;
	// jkrige - viewsize & statusbar


	if (cl.intermission)
		sb_lines = 0;

	//size /= 100.0;


	// jkrige - scale2d
	if (Scale2DFactor > 1.0f)
	{
		/*h = modelist[(int)vid_mode.value].height - sb_lines;
		r_refdef.vrect.width = modelist[(int)vid_mode.value].width * size;
		if (r_refdef.vrect.width < 96)
		{
			size = 96.0 / modelist[(int)vid_mode.value].width;
			r_refdef.vrect.width = 96;	// min for icons
		}

		r_refdef.vrect.height = modelist[(int)vid_mode.value].height * size;
		if (r_refdef.vrect.height > modelist[(int)vid_mode.value].height - sb_lines)
			r_refdef.vrect.height = modelist[(int)vid_mode.value].height - sb_lines;
		if (r_refdef.vrect.height > modelist[(int)vid_mode.value].height)
			r_refdef.vrect.height = modelist[(int)vid_mode.value].height;

		r_refdef.vrect.x = (modelist[(int)vid_mode.value].width - r_refdef.vrect.width)/2;
		//r_refdef.vrect.y = (h - r_refdef.vrect.height)/2;

		if (full)
			r_refdef.vrect.y = 0;
		else 
			r_refdef.vrect.y = (h - r_refdef.vrect.height)/2;*/


		// jkrige - scr_viewsize & statusbar
		r_refdef.vrect.width = modelist[(int)vid_mode.value].width;
		r_refdef.vrect.height = modelist[(int)vid_mode.value].height;
		r_refdef.vrect.x = 0;
		r_refdef.vrect.y = 0;

		/*r_refdef.vrect.width = modelist[(int)vid_mode.value].width * size;
		r_refdef.vrect.height = modelist[(int)vid_mode.value].height * size;

		r_refdef.vrect.x = (modelist[(int)vid_mode.value].width - r_refdef.vrect.width) / 2;
		if(full)
			r_refdef.vrect.y = 0;
		else
			r_refdef.vrect.y = (modelist[(int)vid_mode.value].height - r_refdef.vrect.height) / 2;*/
		// jkrige - scr_viewsize & statusbar
	}
	else
	{
		/*h = vid.height - sb_lines;
		r_refdef.vrect.width = vid.width * size;
		if (r_refdef.vrect.width < 96)
		{
			size = 96.0 / vid.width;
			r_refdef.vrect.width = 96;	// min for icons
		}

		r_refdef.vrect.height = vid.height * size;
		if (r_refdef.vrect.height > vid.height - sb_lines)
			r_refdef.vrect.height = vid.height - sb_lines;
		if (r_refdef.vrect.height > vid.height)
			r_refdef.vrect.height = vid.height;

		r_refdef.vrect.x = (vid.width - r_refdef.vrect.width)/2;
		//r_refdef.vrect.y = (h - r_refdef.vrect.height)/2;

		if (full)
			r_refdef.vrect.y = 0;
		else 
			r_refdef.vrect.y = (h - r_refdef.vrect.height)/2;*/


		// jkrige - scr_viewsize & statusbar
		r_refdef.vrect.width = vid.width;
		r_refdef.vrect.height = vid.height;
		r_refdef.vrect.x = 0;
		r_refdef.vrect.y = 0;

		/*r_refdef.vrect.width = vid.width * size;
		r_refdef.vrect.height = vid.height * size;

		r_refdef.vrect.x = (vid.width - r_refdef.vrect.width) / 2;
		if (full)
			r_refdef.vrect.y = 0;
		else
			r_refdef.vrect.y = (vid.height - r_refdef.vrect.height) / 2;*/
		// jkrige - scr_viewsize & statusbar
	}
	// jkrige - scale2d


	// jkrige - scale2d (quake original)
	/*h = vid.height - sb_lines;
	r_refdef.vrect.width = vid.width * size;
	if (r_refdef.vrect.width < 96)
	{
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;	// min for icons
	}

	r_refdef.vrect.height = vid.height * size;
	if (r_refdef.vrect.height > vid.height - sb_lines)
		r_refdef.vrect.height = vid.height - sb_lines;
	if (r_refdef.vrect.height > vid.height)
			r_refdef.vrect.height = vid.height;
	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width)/2;
	if (full)
		r_refdef.vrect.y = 0;
	else 
		r_refdef.vrect.y = (h - r_refdef.vrect.height)/2;*/
	// jkrige - scale2d (quake original)



	// jkrige - field of view (fov) fix
	if (r_refdef.vrect.width == 640 && r_refdef.vrect.height == 480)
		r_refdef.vrect.height -= (sb_lines > 24) ? sb_lines - 24 : sb_lines - 8;

	fov_monitor = ((float)r_refdef.vrect.width / (float)r_refdef.vrect.height) / ((float)BASEWIDTH / (float)BASEHEIGHT);
	//fov_monitor = ((float)vid.width / (float)vid.height) / ((float)BASEWIDTH / (float)BASEHEIGHT);
	fov_width = (2 * atan(fov_monitor * tan(((float)BASEFOV / 2) * (M_PI / 180)))) * (180 / M_PI);

	if (fov_width >= floor(fov_width) + 0.5f)
		fov_width = ceil(fov_width);
	else
		fov_width = floor(fov_width);


	r_refdef.fov_x = fov_width;
	//r_refdef.fov_x = scr_fov.value;

	//r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, vid.width, vid.height);
	// jkrige - field of view (fov) fix


	scr_vrect = r_refdef.vrect;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize.value+10);
	vid.recalc_refdef = 1;
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize.value-10);
	vid.recalc_refdef = 1;
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	// jkrige - field of view (fov) fix
	//Cvar_RegisterVariable (&scr_fov);
	// jkrige - field of view (fov) fix

	Cvar_RegisterVariable (&scr_viewsize);
	Cvar_RegisterVariable (&scr_conspeed);
	Cvar_RegisterVariable (&scr_showram);
	Cvar_RegisterVariable (&scr_showturtle);
	Cvar_RegisterVariable (&scr_showpause);
	Cvar_RegisterVariable (&scr_centertime);
	Cvar_RegisterVariable (&scr_printspeed);
	Cvar_RegisterVariable (&gl_triplebuffer);

//
// register our commands
//
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);

	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);

	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");

	scr_initialized = true;
}



/*
==============
SCR_DrawRam
==============
*/
void SCR_DrawRam (void)
{
	if (!scr_showram.value)
		return;

	if (!r_cache_thrash)
		return;

	Draw_Pic (scr_vrect.x+32, scr_vrect.y, scr_ram);
}

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle (void)
{
	static int	count;
	
	if (!scr_showturtle.value)
		return;

	if (host_frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic (scr_vrect.x, scr_vrect.y, scr_turtle);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	Draw_Pic (scr_vrect.x+64, scr_vrect.y, scr_net);
}

// jkrige - fps counter
/*
==============
SCR_DrawFPS
==============
*/

void SCR_DrawFPS (void)
{
	extern cvar_t r_fps;

	static double lastframetime;
	double t;
	extern int fps_count;
	static int lastfps;
	int x, y;
	char st[80];

	if (!r_fps.value)
		return;

	t = Sys_FloatTime ();

	if ((t - lastframetime) >= 1.0) {
		lastfps = fps_count;
		fps_count = 0;
		lastframetime = t;
	}

	sprintf(st, "%3d FPS", lastfps);

	x = vid.width - strlen(st) * 8 - 8;
	y = 0 ; //vid.height - (sb_lines * (vid.height/240) )- 16;

//	Draw_TileClear(x, y, strlen(st)*16, 16);
	Draw_String(x, y, st);
}
// jkrige - fps counter

/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
	qpic_t	*pic;

	if (!scr_showpause.value)		// turn off for screenshots
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ( (vid.width - pic->width)/2, 
		(vid.height - 48 - pic->height)/2, pic);
}



/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	qpic_t	*pic;

	if (!scr_drawloading)
		return;
		
	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ( (vid.width - pic->width)/2, 
		(vid.height - 48 - pic->height)/2, pic);
}



//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	Con_CheckResize ();
	
	if (scr_drawloading)
		return;		// never a console with loading plaque
		
// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
		scr_conlines = vid.height/2;	// half screen
	else
		scr_conlines = 0;				// none visible
	
	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.value*host_frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.value*host_frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages)
	{
		//Sbar_Changed (); // jkrige - always draw sbar
	}
	else if (clearnotify++ < vid.numpages)
	{
	}
	else
		con_notifylines = 0;
}
	
/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	if (scr_con_current)
	{
		scr_copyeverything = 1;
		Con_DrawConsole (scr_con_current, true);
		clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/* 
================== 
SCR_ScreenShot_f
================== 
*/  
void SCR_ScreenShot_f (void) 
{
	byte		*buffer;
	char		pcxname[80]; 
	char		checkname[MAX_OSPATH];
	int			i, c, temp;

	// jkrige - "shots" directory
	sprintf (checkname, "%s/shots", com_gamedir);
	Sys_mkdir (checkname);
	// jkrige - "shots" directory

// 
// find a file name to save it to 
// 
	// jkrige - "shots" directory
	//strcpy(pcxname,"quake00.tga");
	strcpy(pcxname,"shots/quake00.tga");
	// jkrige - "shots" directory
		
	for (i=0 ; i<=99 ; i++) 
	{
		// jkrige - "shots" directory
		//pcxname[5] = i/10 + '0';
		//pcxname[6] = i%10 + '0';
		pcxname[11] = i/10 + '0';
		pcxname[12] = i%10 + '0';
		// jkrige - "shots" directory

		sprintf (checkname, "%s/%s", com_gamedir, pcxname);
		if (Sys_FileTime(checkname) == -1)
			break;	// file doesn't exist
	} 
	if (i==100) 
	{
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a TGA file\n"); 
		return;
 	}


	buffer = malloc(glwidth*glheight*3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = glwidth&255;
	buffer[13] = glwidth>>8;
	buffer[14] = glheight&255;
	buffer[15] = glheight>>8;
	buffer[16] = 24;	// pixel size

	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer+18 ); 

	// swap rgb to bgr
	c = 18+glwidth*glheight*3;
	for (i=18 ; i<c ; i+=3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}


	// jkrige - gamma
	if ( ( gl_overbright.value > 0 ) && deviceSupportsGamma )
		VG_GammaCorrect( buffer + 18, glwidth * glheight * 3 );


	COM_WriteFile (pcxname, buffer, glwidth*glheight*3 + 18 );

	free (buffer);
	Con_Printf ("Wrote %s\n", pcxname);
} 


//=============================================================================


/*
===============
SCR_BeginLoadingPlaque

================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds (true);

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;
	
// redraw with no console and the loading plaque
	Con_ClearNotify ();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	scr_fullupdate = 0;
	//Sbar_Changed (); // jkrige - always draw sbar
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
	scr_fullupdate = 0;
}

/*
===============
SCR_EndLoadingPlaque

================
*/
void SCR_EndLoadingPlaque (void)
{
	scr_disabled_for_loading = false;
	scr_fullupdate = 0;
	Con_ClearNotify ();
}

//=============================================================================

char	*scr_notifystring;
qboolean	scr_drawdialog;

void SCR_DrawNotifyString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;

	start = scr_notifystring;

	y = vid.height*0.35;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
			Draw_Character (x, y, start[j]);	
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.  
==================
*/
int SCR_ModalMessage (char *text)
{
	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;
 
// draw a fresh screen
	scr_fullupdate = 0;
	scr_drawdialog = true;
	SCR_UpdateScreen ();
	scr_drawdialog = false;
	
	S_ClearBuffer ();		// so dma doesn't loop current sound

	do
	{
		key_count = -1;		// wait for a key down and up
		Sys_SendKeyEvents ();
	} while (key_lastpress != 'y' && key_lastpress != 'n' && key_lastpress != K_ESCAPE);

	scr_fullupdate = 0;
	SCR_UpdateScreen ();

	return key_lastpress == 'y';
}


//=============================================================================

/*
===============
SCR_BringDownConsole

Brings the console down and fades the palettes back to normal
================
*/
void SCR_BringDownConsole (void)
{
	int		i;
	
	scr_centertime_off = 0;
	
	for (i=0 ; i<20 && scr_conlines != scr_con_current ; i++)
		SCR_UpdateScreen ();

	cl.cshifts[0].percent = 0;		// no area contents palette on next frame
	VID_SetPalette (host_basepal);
}

void SCR_TileClear (void)
{
	int tileX, tileY;
	int tileWidth, tileHeight;
	float divFactor;


	// jkrige - scale2d
	if (Scale2DFactor > 1.0f)
		divFactor = Scale2DFactor;
	else
		divFactor = 1.0f;
	// jkrige - scale2d

	tileX = r_refdef.vrect.x / divFactor;
	tileY = r_refdef.vrect.y / divFactor;
	tileWidth = r_refdef.vrect.width / divFactor;
	tileHeight = r_refdef.vrect.height / divFactor;


	if (r_refdef.vrect.x > 0)
	{
		Draw_TileClear (0, 0, tileX, vid.height); // left
		Draw_TileClear (vid.width - tileX, 0,  tileX, vid.height); // right
	}
	if (r_refdef.vrect.y > 0)
	{
		Draw_TileClear (tileX, 0, vid.width - (tileX * 2), tileY); // top
		Draw_TileClear (tileX, tileY + tileHeight, vid.width - (tileX * 2),  vid.height - (tileHeight + tileY)); // bottom
	}
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void SCR_UpdateScreen (void)
{
	// jkrige - scr_viewsize removal
	//static float	oldscr_viewsize;
	// jkrige - scr_viewsize removal

	vrect_t		vrect;

	if (block_drawing)
		return;

	vid.numpages = 2 + gl_triplebuffer.value;

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (scr_disabled_for_loading)
	{
		if (realtime - scr_disabled_time > 60)
		{
			scr_disabled_for_loading = false;
			Con_Printf ("load failed.\n");
		}
		else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return;				// not initialized yet


	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	
	//
	// determine size of refresh window
	//
	// jkrige - field of view (fov) fix
	/*if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}*/
	if (oldfov != fov_width)
	{
		oldfov = fov_width;
		vid.recalc_refdef = true;
	}
	// jkrige - field of view (fov) fix


	// jkrige - scr_viewsize removal
	/*if (oldscreensize != scr_viewsize.value)
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}*/
	// jkrige - scr_viewsize removal

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole ();
	
	V_RenderView ();

	GL_Set2D ();

	// jkrige - 2D polyblend
	R_PolyBlend ();
	// jkrige - 2D polyblend

	//
	// draw any areas not covered by the refresh
	//
	SCR_TileClear ();


	if (scr_drawdialog)
	{
		Sbar_Draw ();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
		scr_copyeverything = true;
	}
	else if (scr_drawloading)
	{
		SCR_DrawLoading ();
		Sbar_Draw ();
	}
	else if (cl.intermission == 1 && key_dest == key_game)
	{
		Sbar_IntermissionOverlay ();
	}
	else if (cl.intermission == 2 && key_dest == key_game)
	{
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	}
	else
	{
		if (crosshair.value)
		{
			// jkrige - scale2d
			//Draw_Character (scr_vrect.x + scr_vrect.width/2, scr_vrect.y + scr_vrect.height/2, '+');
			Draw_Character ((vid.width / 2)-4, (vid.height / 2)-4, '+');
			// jkrige - scale2d
		}
		
		SCR_DrawRam ();
		SCR_DrawNet ();
		SCR_DrawTurtle ();
		SCR_DrawFPS (); // jkrige - fps counter
		SCR_DrawPause ();
		SCR_CheckDrawCenterString ();
		Sbar_Draw ();
		SCR_DrawConsole ();	
		M_Draw ();
	}

	V_UpdatePalette ();

	GL_EndRendering ();

	// jkrige - texture mode
	Draw_TextureMode_f();
	// jkrige - texture mode
}

