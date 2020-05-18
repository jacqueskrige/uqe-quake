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
// disable data conversion warnings

#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>

#include "glext.h" // jkrige - opengl extensions
#include "wglext.h" // jkrige - windows opengl extensions


void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);


#ifdef _WIN32
// Function prototypes for the Texture Object Extension routines
typedef GLboolean (APIENTRY *ARETEXRESFUNCPTR)(GLsizei, const GLuint *,
                    const GLboolean *);
typedef void (APIENTRY *BINDTEXFUNCPTR)(GLenum, GLuint);
typedef void (APIENTRY *DELTEXFUNCPTR)(GLsizei, const GLuint *);
typedef void (APIENTRY *GENTEXFUNCPTR)(GLsizei, GLuint *);
typedef GLboolean (APIENTRY *ISTEXFUNCPTR)(GLuint);
typedef void (APIENTRY *PRIORTEXFUNCPTR)(GLsizei, const GLuint *,
                    const GLclampf *);
typedef void (APIENTRY *TEXSUBIMAGEPTR)(int, int, int, int, int, int, int, int, void *);

extern	BINDTEXFUNCPTR bindTexFunc;
extern	DELTEXFUNCPTR delTexFunc;
extern	TEXSUBIMAGEPTR TexSubImage2DFunc;
#endif


// jkrige - moved to glquake.h
typedef struct
{
	int		texnum;

	qboolean	tex_luma;		// jkrige - luma textures
	qboolean	tex_luma8bit;	// jkrige - fullbright pixels

	char	identifier[64];
	int		width, height;
	qboolean	mipmap;
	int		lhcsum; // jkrige - memleak & texture mismatch
} gltexture_t;

//#define MAX_GLTEXTURES	2048
#define MAX_GLTEXTURES	4096 // jkrige - increased maximum number of opengl textures
// jkrige - moved to glquake.h


extern gltexture_t	gltextures[MAX_GLTEXTURES];
extern int			numgltextures;

// jkrige - luma textures
#define	JK_LUMA_TEX		(MAX_GLTEXTURES)
// jkrige - luma textures

extern	int texture_extension_number;
extern	int		texture_mode;

extern	float	gldepthmin, gldepthmax;


// jkrige - gamma
extern BOOL  ( WINAPI * qwglGetDeviceGammaRamp3DFX)( HDC, LPVOID );
extern BOOL  ( WINAPI * qwglSetDeviceGammaRamp3DFX)( HDC, LPVOID );
// jkrige - gamma


// jkrige - anisotropic filtering
extern qboolean anisotropic_ext;
extern float maximumAnisotrophy;
// jkrige - anisotropic filtering


// jkrige - non power of two
extern qboolean npow2_ext;
// jkrige - non power of two


void GL_Upload32 (unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha);
void GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, qboolean alpha);

// jkrige - bytesperpixel
//int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha);
int GL_LoadTexture (char *identifier, char *textype, int width, int height, byte *data, int mipmap, int alpha, int bytesperpixel);
// jkrige - bytesperpixel

int GL_FindTexture (char *identifier);

// jkrige - .lit colored lights
void GL_SetupLightmapFmt (qboolean check_cmdline);
// jkrige - .lit colored lights

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;

extern glvert_t glv;

extern	int glx, gly, glwidth, glheight;

// jkrige - opengl extensions
//#ifdef _WIN32
//extern	PROC glArrayElementEXT;
//extern	PROC glColorPointerEXT;
//extern	PROC glTexturePointerEXT;
//extern	PROC glVertexPointerEXT;
//#endif
// jkrige - opengl extensions

// r_local.h -- private refresh defs

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle
#define	MAX_LBM_HEIGHT		480

#define TILE_SIZE		128		// size of textures generated by R_GenTiledSurf

#define SKYSHIFT		7
#define	SKYSIZE			(1 << SKYSHIFT)
#define SKYMASK			(SKYSIZE - 1)

#define BACKFACE_EPSILON	0.01


void R_TimeRefresh_f (void);
void R_ReadPointFile_f (void);
texture_t *R_TextureAnimation (texture_t *base);

typedef struct surfcache_s
{
	struct surfcache_s	*next;
	struct surfcache_s 	**owner;		// NULL is an empty chunk of memory
	int					lightadj[MAXLIGHTMAPS]; // checked for strobe flush
	int					dlight;
	int					size;		// including header
	unsigned			width;
	unsigned			height;		// DEBUG only needed for debug
	float				mipscale;
	struct texture_s	*texture;	// checked for animating textures
	byte				data[4];	// width*height elements
} surfcache_t;


typedef struct
{
	pixel_t		*surfdat;	// destination for generated surface
	int			rowbytes;	// destination logical width in bytes
	msurface_t	*surf;		// description for surface to generate
	fixed8_t	lightadj[MAXLIGHTMAPS];
							// adjust for lightmap levels for dynamic lighting
	texture_t	*texture;	// corrected for animating textures
	int			surfmip;	// mipmapped ratio of surface texels / world pixels
	int			surfwidth;	// in mipmapped texels
	int			surfheight;	// in mipmapped texels
} drawsurf_t;


typedef enum {
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2
} ptype_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct particle_s
{
// driver-usable fields
	vec3_t		org;
	float		color;
// drivers never touch the following fields
	struct particle_s	*next;
	vec3_t		vel;
	float		ramp;
	float		die;
	ptype_t		type;
} particle_t;


//====================================================


extern	entity_t	r_worldentity;
extern	qboolean	r_cache_thrash;		// compatability
extern	vec3_t		modelorg, r_entorigin;
extern	entity_t	*currententity;
extern	int			r_visframecount;	// ??? what difs?
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int		c_brush_polys, c_alias_polys;


//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	texture_t	*r_notexture_mip;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	qboolean	envmap;
extern	int	currenttexture;
extern	int	cnttextures[2];

// jkrige - texture mode
extern	int	particletexture_linear;
extern	int	particletexture_point;
// jkrige - texture mode

extern	int	playertextures;

extern	int	skytexturenum;		// index in cl.loadmodel, not gl texture object

extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_waterwarp;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
//extern	cvar_t	r_shadows; // jkrige - removed alias shadows
extern	cvar_t	r_mirroralpha;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;

// jkrige - fix dynamic light shine through
extern	cvar_t	r_dynamic_sidemark;
// jkrige - fix dynamic light shine through

extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern	cvar_t	gl_poly;
extern	cvar_t	gl_texsort;
extern	cvar_t	gl_smoothmodels;
extern	cvar_t	gl_affinemodels;
extern	cvar_t	gl_polyblend;

// jkrige - disabled tjunction removal
//extern	cvar_t	gl_keeptjunctions;
//extern	cvar_t	gl_reporttjunctions;
// jkrige - disabled tjunction removal

// jkrige - flashblend removal
//extern	cvar_t	gl_flashblend;
// jkrige - flashblend removal

extern	cvar_t	gl_nocolors;
extern	cvar_t	gl_skytype; // jkrige - skybox
extern	cvar_t	gl_doubleeyes;

// jkrige - .lit colored lights
extern	int		gl_coloredstatic;
// jkrige - .lit colored lights

extern	int		gl_lightmap_format;
extern	int		gl_solid_format;
extern	int		gl_alpha_format;

extern	cvar_t	gl_max_size;
extern	cvar_t	gl_playermip;

// jkrige - texture mode
extern	cvar_t	gl_texturemode;
// jkrige - texture mode

// jkrige - wireframe
extern	cvar_t	gl_wireframe;
// jkrige - wireframe

// jkrige - .lit colored lights
extern	cvar_t	gl_coloredlight;
extern	cvar_t	gl_lightmapfmt;
// jkrige - .lit colored lights

// jkrige - luma textures
extern	cvar_t	gl_lumatex_render;
// jkrige - luma textures


extern	int			mirrortexturenum;	// quake texturenum, not gltexturenum
extern	qboolean	mirror;
extern	mplane_t	*mirror_plane;

extern	float	r_world_matrix[16];

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

void R_TranslatePlayerSkin (int playernum);
void GL_Bind (int texnum);

// Multitexture
// jkrige - remove multitexture
//#define    TEXTURE0_SGIS				0x835E
//#define    TEXTURE1_SGIS				0x835F
// jkrige - remove multitexture

#ifndef _WIN32
#define APIENTRY /* */
#endif

// jkrige - overbrights
extern cvar_t gl_overbright;
// jkrige - overbrights

// jkrige - remove multitexture
/*typedef void (APIENTRY *lpMTexFUNC) (GLenum, GLfloat, GLfloat);
typedef void (APIENTRY *lpSelTexFUNC) (GLenum);
extern lpMTexFUNC qglMTexCoord2fSGIS;
extern lpSelTexFUNC qglSelectTextureSGIS;

extern qboolean gl_mtexable;

void GL_DisableMultitexture(void);
void GL_EnableMultitexture(void);*/
// jkrige - remove multitexture
