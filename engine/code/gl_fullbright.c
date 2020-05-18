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
// gl_fullbright.c: fullbright pixel processing

// Developed by Jacques Krige
// Ultimate Quake Engine
// http://www.jacqueskrige.com


#include "quakedef.h"


qboolean GL_FullbrightTexture (byte *in, byte *out, int width, int height)
{
	int			i, j;
	int			p;
	int			numpixels;
	qboolean	fbtex;

	numpixels = width * height;
	fbtex = false;
	
	// check if this texture has fullbrights
	for (i = 0; i < numpixels; i++)
	{
		p = in[i];

		if (p > 238 && p != 255)
		{
			fbtex = true;
			break;
		}
	}


	// replace non fullbrights with black
	if (fbtex == true)
	{
		for (i = 0, j = 0; i < numpixels; i++, j+=4)
		{
			if (in[i] < 239)
				in[i] = 0;

			p = in[i];

			if (p == 0)
			{
				out[j+0] = 0;
				out[j+1] = 0;
				out[j+2] = 0;
				out[j+3] = 255;
			}
			else if (p == 255)
			{
				out[j+0] = 0;
				out[j+1] = 0;
				out[j+2] = 0;
				out[j+3] = 0;
			}
			else
			{
				out[j+0] = host_basepal[(p*3)+0];
				out[j+1] = host_basepal[(p*3)+1];
				out[j+2] = host_basepal[(p*3)+2];
				out[j+3] = 255;
			}
		}
	}

	return fbtex;
}
