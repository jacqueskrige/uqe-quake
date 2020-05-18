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
// r_main.c

#include "quakedef.h"

// jkrige - scale2d
#ifdef _WIN32
#include "winquake.h"
#endif
// jkrige - scale2d

entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	envmap;				// true during envmap command capture 

int			currenttexture = -1;		// to avoid unnecessary texture sets

int			cnttextures[2] = {-1, -1};     // cached

int			particletexture_linear;	// little dot for particles (jkrige - was named "particletexture")

// jkrige - texture mode
int			particletexture_point;
// jkrige - texture mode

int			playertextures;		// up to 16 color translated skins

int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror;
mplane_t	*mirror_plane;

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

// jkrige - .lit colored lights
int		gl_coloredstatic;	// used to store what type of static light we loaded in Mod_LoadLighting()
// jkrige - .lit colored lights

void R_MarkLeaves (void);

cvar_t	r_norefresh = {"r_norefresh","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_lightmap = {"r_lightmap","0"};
//cvar_t	r_shadows = {"r_shadows","0"}; // jkrige - removed alias shadows
cvar_t	r_mirroralpha = {"r_mirroralpha","1"};
cvar_t	r_wateralpha = {"r_wateralpha","1"};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};

// jkrige - fix dynamic light shine through
cvar_t	r_dynamic_sidemark = {"r_dynamic_sidemark", "1", true};
// jkrige - fix dynamic light shine through

// jkrige - remove gl_finish
//cvar_t	gl_finish = {"gl_finish","0"};
// jkrige - remove gl_finish

// jkrige - changed gl_clear default value
//cvar_t	gl_clear = {"gl_clear", "0"};
cvar_t	gl_clear = {"gl_clear", "1", true};
// jkrige - changed gl_clear default value

cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_texsort = {"gl_texsort","1"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1"};
cvar_t	gl_affinemodels = {"gl_affinemodels","0"};
cvar_t	gl_polyblend = {"gl_polyblend","1"};

// jkrige - flashblend removal
//cvar_t	gl_flashblend = {"gl_flashblend","1"};
// jkrige - flashblend removal

cvar_t	gl_playermip = {"gl_playermip","0"};
cvar_t	gl_nocolors = {"gl_nocolors","0"};

// jkrige - disabled tjunction removal
//cvar_t	gl_keeptjunctions = {"gl_keeptjunctions","0"};
//cvar_t	gl_reporttjunctions = {"gl_reporttjunctions","0"};
// jkrige - disabled tjunction removal

cvar_t	gl_skytype = {"gl_skytype", "0"}; // jkrige - skybox : 0 = default, 1 = skybox

cvar_t	gl_doubleeyes = {"gl_doubleeys", "1"};

// jkrige - texture mode
cvar_t	gl_texturemode = {"gl_texturemode", "0", true};
// jkrige - texture mode

// jkrige - wireframe
cvar_t	gl_wireframe = {"gl_wireframe", "0"};
// jkrige - wireframe

// jkrige - .lit colored lights
cvar_t	gl_coloredlight = {"gl_coloredlight", "0", true};
// jkrige - .lit colored lights

extern	cvar_t	gl_ztrick;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}


void R_RotateForEntity (entity_t *e)
{
    glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

    glRotatef (e->angles[1],  0, 0, 1);
    glRotatef (-e->angles[0],  0, 1, 0);
    glRotatef (e->angles[2],  1, 0, 0);
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	vec3_t	point;
	mspriteframe_t	*frame;
	float		*up, *right;
	vec3_t		v_forward, v_right, v_up;
	msprite_t		*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
		AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
	{	// normal sprite
		up = vup;
		right = vright;
	}

	glColor3f (1,1,1);

	// jkrige - remove multitexture
	//GL_DisableMultitexture();
	// jkrige - remove multitexture

    GL_Bind(frame->gl_texturenum);

	glEnable (GL_ALPHA_TEST);
	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);
	
	glEnd ();

	glDisable (GL_ALPHA_TEST);
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

// jkrige - removed alias shadows
//vec3_t	shadevector;
// jkrige - removed alias shadows

// jkrige - .lit colored lights
//float	shadelight, ambientlight;
float	shadelight;
// jkrige - .lit colored lights



// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];

// jkrige - static light vector
float cm_pitch;
float lightvec[3] = {0.0f, 0.0f, 0.0f};
// jkrige - static light vector


// jkrige - light lerping
float *shadedots2 = r_avertexnormal_dots[0];
float lightlerpoffset;
// jkrige - light lerping



int	lastposenum;

// jkrige - .lit colored lights
extern vec3_t lightcolor;
// jkrige - .lit colored lights


// jkrige - fullbright pixels
void GL_DrawAliasFrame2 (aliashdr_t *paliashdr, int posenum, int anim, qboolean fullbrights)
{
	float	s, t;
	float 	l;
	int		i, j;
	int		index;
	trivertx_t	*v, *verts;
	int		list;
	int		*order;
	vec3_t	point;
	float	*normal;
	int		count;

	// jkrige - static light vector
	float dir_light;
	// jkrige - static light vector

	// jkrige - light lerping
	float l1, l2, diff;
	// jkrige - light lerping
	
	lastposenum = posenum;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	if (fullbrights == true)
	{
		if (r_fullbright.value)
			return;

		if(gl_lumatex_render.value != 1)
			return;

		GL_Bind (JK_LUMA_TEX + paliashdr->gl_texturenum[currententity->skinnum][anim]);

		glDepthMask (GL_FALSE);
		glEnable(GL_BLEND);

		glBlendFunc (GL_ONE, GL_ONE);
	}

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			glTexCoord2f (((float *)order)[0], ((float *)order)[1]);
			order += 2;

			// normals and vertexes come from the frame list

			if (fullbrights == false)
			{
				// jkrige - static light vector
				dir_light = DotProduct(r_avertexnormals[verts->lightnormalindex], lightvec);
				if (dir_light > 0.0f)
				{
					// jkrige - light lerping
					l1 = (shadedots[verts->lightnormalindex] * shadelight) + dir_light;
					l2 = (shadedots2[verts->lightnormalindex] * shadelight) + dir_light;
					//l = ambientlight + dir_light;
					// jkrige - light lerping
				}
				else
				{
					// jkrige - light lerping
					l1 = shadedots[verts->lightnormalindex] * shadelight;
					l2 = shadedots2[verts->lightnormalindex] * shadelight;
					//l = ambientlight;
					// jkrige - light lerping
				}
				// jkrige - static light vector


				// jkrige - light lerping
				if (l1 != l2)
				{
					if (l1 > l2)
					{
						diff = l1 - l2;
						diff *= lightlerpoffset;
						l = l1 - diff;
					}
					else
					{
						diff = l2 - l1;
						diff *= lightlerpoffset;
						l = l1 + diff;
					}
				}
				else
				{
					l = l1;
				}
				// jkrige - light lerping

				// jkrige - wireframe
				if (gl_wireframe.value)
					l = 1;
				// jkrige - wireframe

				// jkrige - .lit colored lights
				//l = shadedots[verts->lightnormalindex];
				glColor3f (l * lightcolor[0], l * lightcolor[1], l * lightcolor[2]);
				//l = shadedots[verts->lightnormalindex] * shadelight;
				//glColor3f (l, l, l);
				// jkrige - .lit colored lights
			}
			else
			{
				glColor3f (1.0f, 1.0f, 1.0f);
			}

			
			glVertex3f (verts->v[0], verts->v[1], verts->v[2]);
			verts++;
		} while (--count);

		glEnd ();
	}

	if (fullbrights == true)
	{
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glDisable(GL_BLEND);
		glDepthMask (GL_TRUE);
	}
}
// jkrige - fullbright pixels

/*
=============
GL_DrawAliasFrame
=============
*/
void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum, int skinnum, int anim)
{
	GL_DrawAliasFrame2(paliashdr, posenum, anim, false);

	if (paliashdr->skin_luma[skinnum] == true)
	{
		GL_DrawAliasFrame2(paliashdr, posenum, anim, true);
	
		if (paliashdr->skin_luma8bit[skinnum] == false)
			GL_DrawAliasFrame2(paliashdr, posenum, anim, true);
	}
}


/*
=============
GL_DrawAliasShadow
=============
*/
//extern	vec3_t			lightspot;
// jkrige - removed alias shadows
/*void GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{
	float	s, t, l;
	int		i, j;
	int		index;
	trivertx_t	*v, *verts;
	int		list;
	int		*order;
	vec3_t	point;
	float	*normal;
	float	height, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	height = -lheight + 1.0;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
//			height -= 0.001;
			glVertex3fv (point);

			verts++;
		} while (--count);

		glEnd ();
	}	
}*/
// jkrige - removed alias shadows


/*
=================
R_SetupAliasFrame

=================
*/
void R_SetupAliasFrame (int frame, aliashdr_t *paliashdr, int skinnum, int anim)
{
	int				pose, numposes;
	float			interval;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, pose, skinnum, anim);
}


// jkrige - armabody.mdl hack
void R_DrawArmaBodyHack(entity_t *e)
{
	int i;
	int anim;
	aliashdr_t	*paliashdr;

	if (currententity->frame == 97)
		return;

	if (strcmp(currententity->model->name, "progs/armalegs.mdl"))
		return;

	paliashdr = (aliashdr_t *)Mod_Extradata(currententity->model - 1);

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

	glPushMatrix();

	R_RotateForEntity(e);

	if (!strcmp(currententity->model->name, "progs/eyes.mdl") && gl_doubleeyes.value)
	{
		glTranslatef(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));

		// double size of eyes, since they are really hard to see in gl
		glScalef(paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	}
	else
	{
		glTranslatef(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		glScalef(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

	anim = (int)(cl.time * 10) & 3;
	GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (currententity->colormap != vid.colormap && !gl_nocolors.value)
	{
		i = currententity - cl_entities;
		if (i >= 1 && i <= cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
			GL_Bind(playertextures - 1 + i);
	}

	if (gl_smoothmodels.value)
		glShadeModel(GL_SMOOTH);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);


	R_SetupAliasFrame(currententity->frame, paliashdr, currententity->skinnum, anim);


	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glShadeModel(GL_FLAT);
	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);


	glPopMatrix();
}
// jkrige - armabody.mdl hack


/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *e)
{
	int			i, j;
	int			lnum;
	vec3_t		dist;
	float		add;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	trivertx_t	*verts, *v;
	int			index;
	float		s, t, an;
	int			anim;

	// jkrige - light lerping
	float ang_ceil, ang_floor;
	// jkrige - light lerping

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (R_CullBox (mins, maxs))
		return;


	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//
	// get lighting information
	//

	// jkrige - .lit colored lights
	shadelight = R_LightPoint(currententity->origin);
	//ambientlight = shadelight = R_LightPoint (currententity->origin);
	// jkrige - .lit colored lights

	// allways give the gun some light
	// jkrige - reduced amount of minimum light on gun (view weapon)
	//if (e == &cl.viewent && ambientlight < 24)
	//	ambientlight = shadelight = 24;

	// jkrige - .lit colored lights
	if (e == &cl.viewent)
	{
		if (lightcolor[0] < 10)
			lightcolor[0] = 10;
		if (lightcolor[1] < 10)
			lightcolor[1] = 10;
		if (lightcolor[2] < 10)
			lightcolor[2] = 10;

		if(shadelight < 8)
			shadelight = 8;
	}
	//if (e == &cl.viewent && ambientlight < 10)
	//	ambientlight = shadelight = 10;
	// jkrige - .lit colored lights

	// jkrige - reduced amount of minimum light on gun (view weapon)


	// jkrige - glowing rotating items
	if (currententity->model->flags & EF_ROTATE)
	{
		float shadelightdelta = (255.0f - shadelight) / 2.0f;
		lightcolor[0] = lightcolor[1] = lightcolor[2] = shadelight + ((shadelightdelta * sin(cl.time * 3.5f)) + shadelightdelta) / 2.0f;
	}
	// jkrige - glowing rotating items


	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract (currententity->origin, cl_dlights[lnum].origin,	dist);
			add = cl_dlights[lnum].radius - Length(dist);

			// jkrige - .lit colored lights
			if (add > 0)
			{
				lightcolor[0] += add * cl_dlights[lnum].color[0];
				lightcolor[1] += add * cl_dlights[lnum].color[1];
				lightcolor[2] += add * cl_dlights[lnum].color[2];

				//ambientlight += add;
				//ZOID models should be affected by dlights as well
				//shadelight += add;
			}
			// jkrige - .lit colored lights
		}
	}


	// jkrige - static light vector
	// Set up light direction (from above)
    cm_pitch = currententity->angles[PITCH] * ((float)M_PI / 180.0f);
	lightvec[0] = sin(cm_pitch);
    lightvec[2] = cos(cm_pitch);
	// jkrige - static light vector


	// clamp lighting so it doesn't overbright as much
	// jkrige - static light vector
	//if (ambientlight > 128)
	//	ambientlight = 128;
	//if (ambientlight + shadelight > 192)
	//	shadelight = 192 - ambientlight;
	// jkrige - static light vector

	// ZOID: never allow players to go totally black
	i = currententity - cl_entities;
	if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
	{
		// jkrige - .lit colored lights
		if (lightcolor[0] < 10)
			lightcolor[0] = 10;
		if (lightcolor[1] < 10)
			lightcolor[1] = 10;
		if (lightcolor[2] < 10)
			lightcolor[2] = 10;

		if(shadelight < 8)
			shadelight = 8;
		//if (ambientlight < 8)
		//	ambientlight = shadelight = 8;
		// jkrige - .lit colored lights
	}

	// HACK HACK HACK -- no fullbright colors, so make torches full light
	if (
		!strcmp (clmodel->name, "progs/flame2.mdl")
		| !strcmp (clmodel->name, "progs/flame.mdl")
		| !strcmp (clmodel->name, "progs/lavaball.mdl") // jkrige - lavaball fullbright
		| !strcmp (clmodel->name, "progs/bolt.mdl")  // jkrige - lightning bolt fullbright
		| !strcmp (clmodel->name, "progs/bolt2.mdl") // jkrige - lightning bolt fullbright
		| !strcmp (clmodel->name, "progs/bolt3.mdl") // jkrige - lightning bolt fullbright
		| !strcmp (clmodel->name, "progs/k_spike.mdl") // jkrige - death knight spike fullbright
		| !strcmp (clmodel->name, "progs/laser.mdl") // jkrige - enforcer laser fullbright
		| !strcmp (clmodel->name, "progs/v_spike.mdl") // jkrige - vore spike fullbright
		| !strcmp (clmodel->name, "progs/spike.mdl") // jkrige - nailgun spike fullbright
		| !strcmp (clmodel->name, "progs/missile.mdl") // jkrige - rocket launcher missile fullbright
		| !strcmp (clmodel->name, "progs/w_spike.mdl") // jkrige - scrag spike fullbright
		| !strcmp (clmodel->name, "progs/b_g_key.mdl") // jkrige - b_g_key fullbright
		| !strcmp (clmodel->name, "progs/b_s_key.mdl") // jkrige - b_s_key fullbright
		| !strcmp (clmodel->name, "progs/m_g_key.mdl") // jkrige - b_g_key fullbright
		| !strcmp (clmodel->name, "progs/m_s_key.mdl") // jkrige - b_s_key fullbright
		| !strcmp (clmodel->name, "progs/w_g_key.mdl") // jkrige - w_g_key fullbright
		| !strcmp (clmodel->name, "progs/w_s_key.mdl") // jkrige - w_s_key fullbright
		| !strcmp (clmodel->name, "progs/boss.mdl") // jkrige - boss fullbright
		| !strcmp (clmodel->name, "progs/end1.mdl") // jkrige - end1 fullbright
		| !strcmp (clmodel->name, "progs/end2.mdl") // jkrige - end1 fullbright
		| !strcmp (clmodel->name, "progs/end3.mdl") // jkrige - end1 fullbright
		| !strcmp (clmodel->name, "progs/end4.mdl") // jkrige - end1 fullbright
		)
	{
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 256;
		shadelight = 256;
		//ambientlight = shadelight = 256;
	}


	// jkrige - wireframe
	if (gl_wireframe.value)
	{
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 256;
		shadelight = 256;
	}
	// jkrige - wireframe
	

	// jkrige - light lerping
	lightlerpoffset = e->angles[YAW] * (SHADEDOT_QUANT / 360.0);
	ang_ceil = ceil(lightlerpoffset);
	ang_floor = floor(lightlerpoffset);
	
	lightlerpoffset = ang_ceil - lightlerpoffset;
	//shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	shadedots = r_avertexnormal_dots[(int)ang_ceil & (SHADEDOT_QUANT - 1)];
	shadedots2 = r_avertexnormal_dots[(int)ang_floor & (SHADEDOT_QUANT - 1)];
	// jkrige - light lerping


	// jkrige - .lit colored lights
	VectorScale(lightcolor, 1.0f / 192.0f, lightcolor);
	shadelight = shadelight / 192.0;
	// jkrige - .lit colored lights

	
	// jkrige - removed alias shadows
	//an = e->angles[1]/180*M_PI;
	//shadevector[0] = cos(-an);
	//shadevector[1] = sin(-an);
	//shadevector[2] = 1;
	//VectorNormalize (shadevector);
	// jkrige - removed alias shadows

	//
	// locate the proper data
	//
	
	paliashdr = (aliashdr_t *)Mod_Extradata(currententity->model);

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

	// jkrige - remove multitexture
	//GL_DisableMultitexture();
	// jkrige - remove multitexture

	glPushMatrix();


	R_RotateForEntity(e);

	if (!strcmp(clmodel->name, "progs/eyes.mdl") && gl_doubleeyes.value)
	{
		glTranslatef(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));

		// double size of eyes, since they are really hard to see in gl
		glScalef(paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	}
	else
	{
		glTranslatef(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		glScalef(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

	anim = (int)(cl.time * 10) & 3;
	GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (currententity->colormap != vid.colormap && !gl_nocolors.value)
	{
		i = currententity - cl_entities;
		if (i >= 1 && i <= cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
			GL_Bind(playertextures - 1 + i);
	}

	if (gl_smoothmodels.value)
		glShadeModel(GL_SMOOTH);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);


	R_SetupAliasFrame(currententity->frame, paliashdr, currententity->skinnum, anim);


	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glShadeModel(GL_FLAT);
	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);


	glPopMatrix();

	// jkrige - armabody.mdl hack
	R_DrawArmaBodyHack(e);
	// jkrige - armabody.mdl hack


	// jkrige - removed alias shadows
	/*if (r_shadows.value)
	{
		glPushMatrix ();
		R_RotateForEntity (e);
		glDisable (GL_TEXTURE_2D);
		glEnable (GL_BLEND);
		glColor4f (0,0,0,0.5);
		GL_DrawAliasShadow (paliashdr, lastposenum);
		glEnable (GL_TEXTURE_2D);
		glDisable (GL_BLEND);
		glColor4f (1,1,1,1);
		glPopMatrix ();
	}*/
	// jkrige - removed alias shadows

}

//==================================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawAliasModel (currententity);
			break;

		case mod_brush:
			R_DrawBrushModel (currententity);
			break;

		default:
			break;
		}
	}

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_sprite:
			R_DrawSpriteModel (currententity);
			break;
		}
	}
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	// jkrige - weirdly enough most of the code in this function is useless
	//          the min lighting is already handled within the "R_DrawAliasModel" function

	// jkrige - min light level
	//float		ambient[4], diffuse[4];
	//int			j;
	// jkrige - min light level
	
	//int			lnum;
	//vec3_t		dist;
	//float		add;
	//dlight_t	*dl;

	// jkrige - min light level
	//int			ambientlight, shadelight;
	// jkrige - min light level

	if (!r_drawviewmodel.value)
		return;

	// jkrige - removed chase
	//if (chase_active.value)
	//	return;
	// jkrige - removed chase

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	if (cl.items & IT_INVISIBILITY)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	// jkrige - min light level
	//j = R_LightPoint (currententity->origin);
	//if (j < 24)
	//	j = 24;		// allways give some light on gun
	//ambientlight = j;
	//shadelight = j;
	//ambientlight = 0.75f * R_LightPoint(currententity->origin);
	//ambientlight = 0.01f;
	// jkrige - min light level

// add dynamic lights		
	/*for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		dl = &cl_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
			continue;

		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->radius - Length(dist);
		if (add > 0)
			ambientlight += add;
	}*/

	// jkrige - min light level
	//cl.light_level = (ambientlight > 255) ? 255 : ((ambientlight < 18) ? 18 : ambientlight);
	// jkrige - min light level

	//ambient[0] = ambient[1] = ambient[2] = ambient[3] = (float)ambientlight / 128;
	//diffuse[0] = diffuse[1] = diffuse[2] = diffuse[3] = (float)shadelight / 128;

	// hack the depth range to prevent view model from poking into walls
	glDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));
	R_DrawAliasModel (currententity);
	glDepthRange (gldepthmin, gldepthmax);
}


/*
============
R_PolyBlend
============
*/
// jkrige - 2D polyblend
void R_PolyBlend (void)
{
	if (!gl_polyblend.value)
		return;

	if (!v_blend[3])
		return;

	glAlphaFunc(GL_ALWAYS, 0);

	glLoadIdentity ();

	glEnable (GL_BLEND);
	glDisable (GL_TEXTURE_2D);

	glColor4fv (v_blend);

	glBegin (GL_QUADS);
	glVertex2f (0,0);
	glVertex2f (vid.width, 0);
	glVertex2f (vid.width, vid.height);
	glVertex2f (0, vid.height);
	glEnd ();

	glColor4f (1,1,1,1);

	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);

	glAlphaFunc(GL_GREATER, 0.632);
}
/*void R_PolyBlend (void)
{
	if (!gl_polyblend.value)
		return;
	if (!v_blend[3])
		return;

	// jkrige - remove multitexture
	//GL_DisableMultitexture();
	// jkrige - remove multitexture

	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_TEXTURE_2D);

    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up

	glColor4fv (v_blend);

	glBegin (GL_QUADS);

	glVertex3f (10, 100, 100);
	glVertex3f (10, -100, 100);
	glVertex3f (10, -100, -100);
	glVertex3f (10, 100, -100);
	glEnd ();

	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);
}*/
// jkrige - 2D polyblend


int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

	if (r_refdef.fov_x == 90) 
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	}
	else
	{
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );
	}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}



/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	int				edgecount;
	vrect_t			vrect;
	float			w, h;

// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_Set ("r_fullbright", "0");

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;

}


void MYgluPerspective( GLdouble fovy, GLdouble aspect,
		     GLdouble zNear, GLdouble zFar )
{
   GLdouble xmin, xmax, ymin, ymax;

   ymax = zNear * tan( fovy * M_PI / 360.0 );
   ymin = -ymax;

   xmin = ymin * aspect;
   xmax = ymax * aspect;

   glFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	float	yfov;
	int		i;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;

	// jkrige - water warp (contents)
	double f;
	// jkrige - water warp (contents)


	//
	// set up viewpoint
	//
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();

	// jkrige - scale2d
	//x = r_refdef.vrect.x * glwidth/vid.width;
	//x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	//y = (vid.height-r_refdef.vrect.y) * glheight/vid.height;
	//y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;
	if (Scale2DFactor > 1.0f)
	{
		x = r_refdef.vrect.x * glwidth/modelist[(int)vid_mode.value].width /*320*/;
		x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/modelist[(int)vid_mode.value].width /*320*/;
		y = (modelist[(int)vid_mode.value].height /*200*/ -r_refdef.vrect.y) * glheight/modelist[(int)vid_mode.value].height /*200*/;
		y2 = (modelist[(int)vid_mode.value].height /*200*/ - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/modelist[(int)vid_mode.value].height /*200*/;
	}
	else
	{
		// jkrige - scale2d (original)
		x = r_refdef.vrect.x * glwidth/vid.width /*320*/;
		x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width /*320*/;
		y = (vid.height/*200*/-r_refdef.vrect.y) * glheight/vid.height /*200*/;
		y2 = (vid.height/*200*/ - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height /*200*/;
		// jkrige - scale2d (original)
	}
	// jkrige - scale2d


	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	glViewport (glx + x, gly + y2, w, h);

	// jkrige - field of view (fov) fix
    //screenaspect = (float)r_refdef.vrect.width/(float)r_refdef.vrect.height;
	//yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
	screenaspect = (float)vid.width/(float)vid.height;
	yfov = r_refdef.fov_y;
	// jkrige - field of view (fov) fix


	// jkrige - water warp (contents)
	if ((r_viewleaf->contents == CONTENTS_WATER || r_viewleaf->contents == CONTENTS_LAVA || r_viewleaf->contents == CONTENTS_SLIME))
	{
		f = sin(realtime * 0.15 * (M_PI * 2.7));
		screenaspect = screenaspect - (1 - f) * 0.05 * 1;
		yfov = r_refdef.fov_y - (1 + f) * 1;
	}
	// jkrige - water warp (contents)


	// jkrige - updated near & far planes
    MYgluPerspective (yfov,  screenaspect,  2,  6144);
	//MYgluPerspective (r_refdef.fov_y,  screenaspect,  4,  4096);
	// jkrige - updated near & far planes


	if (mirror)
	{
		if (mirror_plane->normal[2])
			glScalef (1, -1, 1);
		else
			glScalef (-1, 1, 1);
		glCullFace(GL_BACK);
	}
	else
		glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up
    glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
    glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
    glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
    glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList ();

	// jkrige - remove multitexture
	//GL_DisableMultitexture();
	// jkrige - remove multitexture

	// jkrige - flashblend removal
	//R_RenderDlights ();
	// jkrige - flashblend removal

	R_DrawParticles ();

#ifdef GLTEST
	Test_Draw ();
#endif

}


/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	if (r_mirroralpha.value != 1.0)
	{
		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 0.5;
		glDepthFunc (GL_LEQUAL);
	}
	else if (gl_ztrick.value)
	{
		static int trickframe;

		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			glDepthFunc (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			glDepthFunc (GL_GEQUAL);
		}
	}
	else
	{
		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc (GL_LEQUAL);
	}

	glDepthRange (gldepthmin, gldepthmax);
}

/*
=============
R_Mirror
=============
*/
void R_Mirror (void)
{
	float		d;
	msurface_t	*s;
	entity_t	*ent;

	if (!mirror)
		return;

	memcpy (r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2*d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct (vpn, mirror_plane->normal);
	VectorMA (vpn, -2*d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asin (vpn[2])/M_PI*180;
	r_refdef.viewangles[1] = atan2 (vpn[1], vpn[0])/M_PI*180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = &cl_entities[cl.viewentity];
	if (cl_numvisedicts < MAX_VISEDICTS)
	{
		cl_visedicts[cl_numvisedicts] = ent;
		cl_numvisedicts++;
	}

	gldepthmin = 0.5;
	gldepthmax = 1;
	glDepthRange (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);

	R_RenderScene ();
	R_DrawWaterSurfaces ();

	gldepthmin = 0;
	gldepthmax = 0.5;
	glDepthRange (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);

	// blend on top
	glEnable (GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	if (mirror_plane->normal[2])
		glScalef (1,-1,1);
	else
		glScalef (-1,1,1);
	glCullFace(GL_FRONT);
	glMatrixMode(GL_MODELVIEW);

	glLoadMatrixf (r_base_world_matrix);

	glColor4f (1,1,1,r_mirroralpha.value);
	s = cl.worldmodel->textures[mirrortexturenum]->texturechain;
	for ( ; s ; s=s->texturechain)
		R_RenderBrushPoly (s);
	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;
	glDisable (GL_BLEND);
	glColor4f (1,1,1,1);
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	double	time1, time2;
	GLfloat colors[4] = {(GLfloat) 0.0, (GLfloat) 0.0, (GLfloat) 1, (GLfloat) 0.20};

	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		glFinish ();
		time1 = Sys_FloatTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = false;

	// jkrige - remove gl_finish
	//if (gl_finish.value)
	//	glFinish ();
	// jkrige - remove gl_finish

	// jkrige - scale2d
	COM_SetScale2D();
	// jkrige - scale2d

	R_Clear ();

	// render normal view

	/***** Experimental silly looking fog ******
	****** Use r_fullbright if you enable ******
		glFogi(GL_FOG_MODE, GL_LINEAR);
		glFogfv(GL_FOG_COLOR, colors);
		glFogf(GL_FOG_END, 512.0);
		glEnable(GL_FOG);
	********************************************/


	// jkrige - wireframe
	if (gl_wireframe.value != 0.0f && gl_wireframe.value != 1.0f)
		Cvar_Set("gl_wireframe", "0");

	if (cl.gametype == GAME_DEATHMATCH)
		Cvar_Set("gl_wireframe", "0");

	if (gl_wireframe.value)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	// jkrige - wireframe


	R_RenderScene ();
	R_DrawViewModel ();
	R_DrawWaterSurfaces ();

//  More fog right here :)
//	glDisable(GL_FOG);
//  End of all fog code...

	// render mirror view
	R_Mirror ();

	// jkrige - 2D polyblend
	//R_PolyBlend ();
	// jkrige - 2D polyblend

	// jkrige - wireframe
	if (gl_wireframe.value)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	// jkrige - wireframe

	if (r_speeds.value)
	{
//		glFinish ();
		time2 = Sys_FloatTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2-time1)*1000), c_brush_polys, c_alias_polys); 
	}
}
