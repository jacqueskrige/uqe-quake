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
// gl_warp.c -- sky and water polygons

#include "quakedef.h"

extern	model_t	*loadmodel;

int		skytexturenum;

int		solidskytexture;
int		alphaskytexture;
float	speedscale;		// for top sky and bottom sky

msurface_t	*warpface;

// jkrige - quake2 warps
//extern cvar_t gl_subdivide_size;
#define	SUBDIVIDE_SIZE	64
// jkrige - quake2 warps

void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

// jkrige - quake2 warps
void SubdividePolygon (int numverts, float *verts)
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float	*v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t	*poly;
	float	s, t;
	vec3_t	total;
	float	total_s, total_t;

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	//if (numverts > 60)
	//	ri.Sys_Error (ERR_DROP, "numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = SUBDIVIDE_SIZE * floor (m/SUBDIVIDE_SIZE + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j=0 ; j<numverts ; j++, v+= 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v-=i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numverts ; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j+1] > 0) )
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j+1]);
				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	// add a point in the center to help keep warp valid
	poly = Hunk_Alloc (sizeof(glpoly_t) + ((numverts-4)+2) * VERTEXSIZE*sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts+2;
	VectorClear (total);
	total_s = 0;
	total_t = 0;
	for (i=0 ; i<numverts ; i++, verts+= 3)
	{
		VectorCopy (verts, poly->verts[i+1]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);

		total_s += s;
		total_t += t;
		VectorAdd (total, verts, total);

		poly->verts[i+1][3] = s;
		poly->verts[i+1][4] = t;
	}

	VectorScale (total, (1.0/numverts), poly->verts[0]);
	poly->verts[0][3] = total_s/numverts;
	poly->verts[0][4] = total_t/numverts;

	// copy first vertex to last
	memcpy (poly->verts[i+1], poly->verts[1], sizeof(poly->verts[0]));
}

/*void SubdividePolygon (int numverts, float *verts)
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float	*v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t	*poly;
	float	s, t;

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = gl_subdivide_size.value * floor (m/gl_subdivide_size.value + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j=0 ; j<numverts ; j++, v+= 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v-=i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numverts ; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j+1] > 0) )
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j+1]);
				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly = Hunk_Alloc (sizeof(glpoly_t) + (numverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i=0 ; i<numverts ; i++, verts+= 3)
	{
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}*/
// jkrige - quake2 warps

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface (msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;
	//texture_t	*t;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon (numverts, verts[0]);
}

//=========================================================



// speed up sin calculations - Ed
// jkrige - quake2 warps
/*float	turbsin[] =
{
	#include "gl_warp_sin.h"
};*/
// jkrige - quake2 warps
#define TURBSCALE (256.0 / (2 * M_PI))

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
// jkrige - quake2 warps
void EmitWaterPolys (msurface_t *fa)
{
	glpoly_t	*p, *bp;
	float		*v;
	int			i;
	float		s, t, os, ot;
	//float rdt = realtime;

	for (bp=fa->polys ; bp ; bp=bp->next)
	{
		p = bp;

		glBegin (GL_TRIANGLE_FAN);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			os = v[3];
			ot = v[4];

			//s = os + turbsin[(int)((ot * 0.125 + realtime) * TURBSCALE) & 255];
			s = os + sin(DEG2RAD((ot * 0.125 + realtime) * TURBSCALE)) * 3.7;
			s *= (1.0/64);

			//t = ot + turbsin[(int)((os * 0.125 + realtime) * TURBSCALE) & 255];
			t = ot + sin(DEG2RAD((os * 0.125 + realtime) * TURBSCALE)) * 3.7;
			t *= (1.0/64);

			glTexCoord2f (s, t);
			glVertex3fv (v);
		}
		glEnd ();
	}
}

/*void EmitWaterPolys (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float		s, t, os, ot;


	for (p=fa->polys ; p ; p=p->next)
	{
		glBegin (GL_POLYGON);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			os = v[3];
			ot = v[4];

			s = os + turbsin[(int)((ot*0.125+realtime) * TURBSCALE) & 255];
			s *= (1.0/64);

			t = ot + turbsin[(int)((os*0.125+realtime) * TURBSCALE) & 255];
			t *= (1.0/64);

			glTexCoord2f (s, t);
			glVertex3fv (v);
		}
		glEnd ();
	}
}*/
// jkrige - quake2 warps



/*
=============
EmitSkyPolys
=============
*/
void EmitSkyPolys (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float	s, t;
	vec3_t	dir;
	float	length;

	// jkrige - wireframe
	if (gl_wireframe.value)
		return;
	// jkrige - wireframe

	for (p=fa->polys ; p ; p=p->next)
	{
		glBegin (GL_POLYGON);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			VectorSubtract (v, r_origin, dir);
			dir[2] *= 3;	// flatten the sphere

			length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
			length = sqrt (length);
			length = 6*63/length;

			dir[0] *= length;
			dir[1] *= length;

			s = (speedscale + dir[0]) * (1.0/128);
			t = (speedscale + dir[1]) * (1.0/128);

			glTexCoord2f (s, t);
			glVertex3fv (v);
		}
		glEnd ();
	}
}

/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void EmitBothSkyLayers (msurface_t *fa)
{
	int			i;
	int			lindex;
	float		*vec;

	// jkrige - remove multitexture
	//GL_DisableMultitexture();
	// jkrige - remove multitexture

	// jkrige - wireframe
	if (gl_wireframe.value)
		return;
	// jkrige - wireframe

	GL_Bind (solidskytexture);
	speedscale = realtime*8;
	speedscale -= (int)speedscale & ~127 ;

	EmitSkyPolys (fa);

	glEnable (GL_BLEND);
	GL_Bind (alphaskytexture);
	speedscale = realtime*16;
	speedscale -= (int)speedscale & ~127 ;

	EmitSkyPolys (fa);

	glDisable (GL_BLEND);
}

#ifndef QUAKE2
/*
=================
R_DrawSkyChain
=================
*/
// jkrige - skybox
/*void R_DrawSkyChain (msurface_t *s)
{
	msurface_t	*fa;

	// jkrige - remove multitexture
	//GL_DisableMultitexture();
	// jkrige - remove multitexture

	// used when gl_texsort is on
	GL_Bind(solidskytexture);
	speedscale = realtime*8;
	speedscale -= (int)speedscale & ~127 ;

	for (fa=s ; fa ; fa=fa->texturechain)
		EmitSkyPolys (fa);

	glEnable (GL_BLEND);
	GL_Bind (alphaskytexture);
	speedscale = realtime*16;
	speedscale -= (int)speedscale & ~127 ;

	for (fa=s ; fa ; fa=fa->texturechain)
		EmitSkyPolys (fa);

	glDisable (GL_BLEND);
}*/
// jkrige - skybox
#endif

/*
=================================================================

  Quake 2 environment sky

=================================================================
*/

//#ifdef QUAKE2 // jkrige - skybox


#define	SKY_TEX		2560 // jkrige - skybox

/*
==================
R_LoadSkys
==================
*/
char	*suf[6] = {"_rt", "_bk", "_lf", "_ft", "_up", "_dn"};
// jkrige - skybox
/*void R_LoadSkys (void)
{
	int		i;
	FILE	*f;
	char	name[64];

	for (i=0 ; i<6 ; i++)
	{
		GL_Bind (SKY_TEX + i);
		sprintf (name, "gfx/env/bkgtst%s.tga", suf[i]);
		COM_FOpenFile (name, &f, false);
		if (!f)
		{
			Con_Printf ("Couldn't load %s\n", name);
			continue;
		}
		LoadTGA (f);
//		LoadPCX (f);

		glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, targa_rgba);
//		glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pcx_rgb);

		free (targa_rgba);
//		free (pcx_rgb);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}*/

void R_LoadSkys (void)
{
	int		i;
	char	name[64];
	byte	*data;

	for (i=0 ; i<6 ; i++)
	{
		sprintf (name, "skies/%s%s", cl.skybox, suf[i]);
		if (!(data = LoadImagePixels (name, true)))
		{
			Cvar_Set("gl_skytype", "0");
			return;
		}

		GL_Bind (SKY_TEX + i);

		if(image_bits == 8)
			GL_Upload8 (data, image_width, image_height, false, false);
		if(image_bits == 32)
			GL_Upload32 ((void*)data, image_width, image_height, false, false);

		free(data);

		// jkrige - texture mode
		if(gl_texturemode.value == 0.0f)
		{
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		else
		{
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		// jkrige - texture mode
	}

	Cvar_Set("gl_skytype", "1");
}
// jkrige - skybox


vec3_t	skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1} 
};
int	c_sky;

// 1 = s, 2 = t, 3 = 2048
int	st_to_vec[6][3] =
{
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down

//	{-1,2,3},
//	{1,2,-3}
};

// s = [0]/[2], t = [1]/[2]
int	vec_to_st[6][3] =
{
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}

//	{-1,2,3},
//	{1,2,-3}
};

#define	MAX_CLIP_VERTS	64
// jkrige - skybox (clipping)
//float	skymins[2][6], skymaxs[2][6]; // jkrige - skybox

/*void DrawSkyPolygon (int nump, vec3_t vecs)
{
	int		i,j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

	c_sky++;
#if 0
glBegin (GL_POLYGON);
for (i=0 ; i<nump ; i++, vecs+=3)
{
	VectorAdd(vecs, r_origin, v);
	glVertex3fv (v);
}
glEnd();
return;
#endif
	// decide which face it maps to
	VectorCopy (vec3_origin, v);
	for (i=0, vp=vecs ; i<nump ; i++, vp+=3)
	{
		VectorAdd (vp, v, v);
	}
	av[0] = fastfabs(v[0]);
	av[1] = fastfabs(v[1]);
	av[2] = fastfabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i=0 ; i<nump ; i++, vecs+=3)
	{
		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];

		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j -1] / dv;
		else
			s = vecs[j-1] / dv;
		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j -1] / dv;
		else
			t = vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
	float	*norm;
	float	*v;
	qboolean	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if (nump > MAX_CLIP_VERTS-2)
		Sys_Error ("ClipSkyPolygon: MAX_CLIP_VERTS");
	if (stage == 6)
	{	// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*3)) );
	newc[0] = newc[1] = 0;

	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{
			e = v[j] + d*(v[j+3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}*/
// jkrige - skybox (clipping)

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain (msurface_t *s)
{
	msurface_t	*fa;

	int		i;
	vec3_t	verts[MAX_CLIP_VERTS];
	glpoly_t	*p;

	// jkrige - wireframe
	if (gl_wireframe.value)
		return;
	// jkrige - wireframe

	if(!strcmpi(cl.skybox, "") | gl_skytype.value == 0)
	{
		// used when gl_texsort is on
		GL_Bind(solidskytexture);
		speedscale = realtime*8;
		speedscale -= (int)speedscale & ~127 ;

		for (fa=s ; fa ; fa=fa->texturechain)
			EmitSkyPolys (fa);

		glEnable (GL_BLEND);
		GL_Bind (alphaskytexture);
		speedscale = realtime*16;
		speedscale -= (int)speedscale & ~127;

		for (fa=s ; fa ; fa=fa->texturechain)
			EmitSkyPolys (fa);

		glDisable (GL_BLEND);
	}
}


/*
==============
R_ClearSkyBox
==============
*/
// jkrige - skybox (clipping)
/*void R_ClearSkyBox (void)
{
	int		i;

	for (i=0 ; i<6 ; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}*/
// jkrige - skybox (clipping)


void MakeSkyVec (float s, float t, int axis)
{
	vec3_t		v, b;
	int			j, k;

	// jkrige - skybox (enlarged)
	/*b[0] = s*2048;
	b[1] = t*2048;
	b[2] = 2048;*/
	b[0] = s*2360;
	b[1] = t*2360;
	b[2] = 2360;
	// jkrige - skybox (enlarged)

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
		v[j] += r_origin[j];
	}

	// avoid bilerp seam
	s = (s+1)*0.5;
	t = (t+1)*0.5;

	if (s < 1.0/512)
		s = 1.0/512;
	else if (s > 511.0/512)
		s = 511.0/512;
	if (t < 1.0/512)
		t = 1.0/512;
	else if (t > 511.0/512)
		t = 511.0/512;

	t = 1.0 - t;
	glTexCoord2f (s, t);
	glVertex3fv (v);
}

/*
==============
R_DrawSkyBox
==============
*/
int	skytexorder[6] = {0,2,1,3,4,5};
void R_DrawSkyBox (void)
{
	int		i, j, k;
	vec3_t	v;
	float	s, t;

	if(gl_skytype.value != 1)
		return;

	// jkrige - wireframe
	if (gl_wireframe.value)
		return;
	// jkrige - wireframe

//#if 0
//glEnable (GL_BLEND);
//glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
//glColor4f (1,1,1,0.5);
glDisable (GL_DEPTH_TEST); // jkrige - skybox (disabled depth ckecking)
//#endif

	//glGetIntegerv(GL_DEPTH_FUNC, &gld);
	//glDepthFunc(GL_GREATER);
//glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
//glDisable (GL_DEPTH_TEST);
	for (i=0 ; i<6 ; i++)
	{
		//if (skymins[0][i] >= skymaxs[0][i]
		//|| skymins[1][i] >= skymaxs[1][i])
		//	continue;

		GL_Bind (SKY_TEX+skytexorder[i]);

//#if 0 // jkrige - skybox (enabled skybox with fixed mins/maxs)
		//if(gl_fullskybox.value == 1)
		//{
		//	skymins[0][i] = -1;
		//	skymins[1][i] = -1;
		//	skymaxs[0][i] = 1;
		//	skymaxs[1][i] = 1;
		//}
//#endif
		glBegin (GL_QUADS);
		MakeSkyVec (-1 /*skymins[0][i]*/, -1 /*skymins[1][i]*/, i);
		MakeSkyVec (-1 /*skymins[0][i]*/, 1  /*skymaxs[1][i]*/, i);
		MakeSkyVec (1  /*skymaxs[0][i]*/, 1  /*skymaxs[1][i]*/, i);
		MakeSkyVec (1  /*skymaxs[0][i]*/, -1 /*skymins[1][i]*/, i);
		glEnd ();
	}
	//glDepthFunc(gld);
//#if 0
//glDisable (GL_BLEND);
//glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
//glColor4f (1,1,1,0.5);
glEnable (GL_DEPTH_TEST); // jkrige - skybox (disabled depth ckecking)
//#endif
//glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
//glEnable (GL_DEPTH_TEST);

	// jkrige - skybox test
	//restore matrix and rendering state
	//glPopMatrix();
	//glPopAttrib();
	// jkrige - skybox test
}

//#endif // jkrige - skybox



//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (texture_t *mt)
{
	int			i, j, p;
	byte		*src;
	unsigned	trans[128*128];
	unsigned	transpix;
	int			r, g, b;
	unsigned	*rgba;
	extern	int			skytexturenum;


	// jkrige - external texture loading
	FILE				*f2;
	//qboolean			ExtOK = false;
	int					FoundSolid = -1;
	int					FoundAlpha = -1;
	char				skytex_solid[MAX_QPATH];
	char				skytex_alpha[MAX_QPATH];
	byte			*skydata;

	// find solid sky texture
	if(FoundSolid == -1)
	{
		sprintf (skytex_solid, "textures/%s/%s_solid.tga", sv.name, mt->name);
		FoundSolid = COM_FOpenFile(skytex_solid, &f2);
	}
	if(FoundSolid == -1)
	{
		sprintf (skytex_solid, "textures/%s_solid.tga", mt->name);
		FoundSolid = COM_FOpenFile(skytex_solid, &f2);
	}
	if(FoundSolid == -1)
	{
		sprintf (skytex_solid, "textures/%s/%s_solid.jpg", sv.name, mt->name);
		FoundSolid = COM_FOpenFile(skytex_solid, &f2);
	}
	if(FoundSolid == -1)
	{
		sprintf (skytex_solid, "textures/%s_solid.jpg", mt->name);
		FoundSolid = COM_FOpenFile(skytex_solid, &f2);
	}

	// find alpha sky texture
	if(FoundAlpha == -1)
	{
		sprintf (skytex_alpha, "textures/%s/%s_alpha.tga", sv.name, mt->name);
		FoundAlpha = COM_FOpenFile(skytex_alpha, &f2);
	}
	if(FoundAlpha == -1)
	{
		sprintf (skytex_alpha, "textures/%s_alpha.tga", mt->name);
		FoundAlpha = COM_FOpenFile(skytex_alpha, &f2);
	}
	if(FoundAlpha == -1)
	{
		sprintf (skytex_alpha, "textures/%s/%s_alpha.jpg", sv.name, mt->name);
		FoundAlpha = COM_FOpenFile(skytex_alpha, &f2);
	}
	if(FoundAlpha == -1)
	{
		sprintf (skytex_alpha, "textures/%s_alpha.jpg", mt->name);
		FoundAlpha = COM_FOpenFile(skytex_alpha, &f2);
	}


	if(FoundSolid != -1 && FoundAlpha != -1)
	{
		// load solid sky texture
		if ((skydata = LoadImagePixels (skytex_solid, false)))
		{
			if (!solidskytexture)
				solidskytexture = texture_extension_number++;
			
			GL_Bind (solidskytexture);
			glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, skydata);

			if(gl_texturemode.value == 0.0f)
			{
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}
			else
			{
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}

			free(skydata);
		}

		// load alpha sky texture
		if ((skydata = LoadImagePixels (skytex_alpha, false)))
		{
			if (!alphaskytexture)
				alphaskytexture = texture_extension_number++;
			
			GL_Bind(alphaskytexture);
			glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, skydata);

			if(gl_texturemode.value == 0.0f)
			{
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}
			else
			{
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}

			free(skydata);
		}

		return;
	}
	// jkrige - external texture loading


	src = (byte *)mt + mt->offsets[0];

	// make an average value for the back to avoid
	// a fringe on the top level

	r = g = b = 0;
	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i*128) + j] = *rgba;
			r += ((byte *)rgba)[0];
			g += ((byte *)rgba)[1];
			b += ((byte *)rgba)[2];
		}

	((byte *)&transpix)[0] = r/(128*128);
	((byte *)&transpix)[1] = g/(128*128);
	((byte *)&transpix)[2] = b/(128*128);
	((byte *)&transpix)[3] = 0;


	if (!solidskytexture)
		solidskytexture = texture_extension_number++;
	GL_Bind (solidskytexture );
	glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

	// jkrige - texture mode
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if(gl_texturemode.value == 0.0f)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	// jkrige - texture mode


	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j];
			if (p == 0)
				trans[(i*128) + j] = transpix;
			else
				trans[(i*128) + j] = d_8to24table[p];
		}

	if (!alphaskytexture)
		alphaskytexture = texture_extension_number++;
	GL_Bind(alphaskytexture);
	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

	// jkrige - texture mode
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if(gl_texturemode.value == 0.0f)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	// jkrige - texture mode
}

