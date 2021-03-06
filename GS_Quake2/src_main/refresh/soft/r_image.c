/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "r_local.h"


#define	MAX_RIMAGES	1024
image_t		r_images[MAX_RIMAGES];
int		numr_images;


/*
===============
R_ImageList_f
===============
*/
void	R_ImageList_f (void)
{
	int		i;
	image_t	*image;
	int		texels;

	Com_Printf( "------------------\n");
	texels = 0;

	for (i=0, image=r_images ; i<numr_images ; i++, image++)
	{
		if (image->registration_sequence <= 0)
			continue;
		texels += image->width*image->height;
		switch (image->type)
		{
		case it_skin:
			Com_Printf( "M");
			break;
		case it_sprite:
			Com_Printf( "S");
			break;
		case it_wall:
			Com_Printf( "W");
			break;
		case it_pic:
			Com_Printf( "P");
			break;
		default:
			Com_Printf( " ");
			break;
		}

		Com_Printf(  " %3i %3i : %s\n",
			image->width, image->height, image->name);
	}
	Com_Printf( "Total texel count: %i\n", texels);
}

//=======================================================

static image_t *R_FindFreeImage (void)
{
	image_t		*image;
	int			i;

	// find a free image_t
	for (i=0, image=r_images ; i<numr_images ; i++,image++)
	{
		if (!image->registration_sequence)
			break;
	}
	if (i == numr_images)
	{
		if (numr_images == MAX_RIMAGES)
			Com_Error (ERR_DROP, "MAX_RIMAGES");
		numr_images++;
	}
	image = &r_images[i];

	return image;
}

/*
================
R_LoadPic
================
*/
image_t *R_LoadPic (char *name, byte *pic, int width, int height, imagetype_t type)
{
	image_t *image;
	int i, c;

	image = R_FindFreeImage ();
	if (strlen(name) >= sizeof(image->name))
		Com_Error(ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);
	strcpy (image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;

	c = width*height;
	image->pixels[0] = malloc (c);
	image->transparent = false;
	for (i=0 ; i<c ; i++)
	{
		int b;

		b = pic[i];
		if (b == 255)
			image->transparent = true;
		image->pixels[0][i] = b;
	}

	return image;
}

/*
================
R_LoadWal
================
*/
image_t *R_LoadWal (char *name)
{
	miptex_t	*mt;
	int			ofs;
	image_t		*image;
	int			size;

	FS_LoadFile (name, (void **)&mt);
	if (!mt)
	{
		Com_Printf( "R_LoadWal: can't load %s\n", name);
		return r_notexture_mip;
	}

	image = R_FindFreeImage ();
	strcpy (image->name, name);
	image->width = LittleLong (mt->width);
	image->height = LittleLong (mt->height);
	image->type = it_wall;
	image->registration_sequence = registration_sequence;

	size = image->width*image->height * (256+64+16+4)/256;
	image->pixels[0] = malloc (size);
	image->pixels[1] = image->pixels[0] + image->width*image->height;
	image->pixels[2] = image->pixels[1] + image->width*image->height/4;
	image->pixels[3] = image->pixels[2] + image->width*image->height/16;

	ofs = LittleLong (mt->offsets[0]);
	memcpy ( image->pixels[0], (byte *)mt + ofs, size);

	FS_FreeFile ((void *)mt);

	return image;
}

/*
===============
SW_LoadPCX
===============
*/
void SW_LoadPCX(char *origname, byte **pic, byte **palette, int *width, int *height)
{
	byte *raw;
	pcx_t *pcx;
	int x, y;
	int len, full_size;
	int pcx_width, pcx_height;
	qboolean image_issues = false;
	int dataByte, runLength;
	byte *out, *pix;
	char filename[256];

	Q_strlcpy(filename, origname, sizeof(filename));

	/* Add the extension */
	if (strcmp(COM_FileExtension(filename), "pcx"))
	{
		Q_strlcat(filename, ".pcx", sizeof(filename));
	}

	*pic = NULL;

	if (palette)
	{
		*palette = NULL;
	}

	/* load the file */
	len = FS_LoadFile(filename, (void **)&raw);

	if (!raw || len < sizeof(pcx_t))
	{
		Com_DPrintf("Bad pcx file %s\n", filename);
		return;
	}

	/* parse the PCX file */
	pcx = (pcx_t *)raw;

	pcx->xmin = LittleShort(pcx->xmin);
	pcx->ymin = LittleShort(pcx->ymin);
	pcx->xmax = LittleShort(pcx->xmax);
	pcx->ymax = LittleShort(pcx->ymax);
	pcx->hres = LittleShort(pcx->hres);
	pcx->vres = LittleShort(pcx->vres);
	pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
	pcx->palette_type = LittleShort(pcx->palette_type);

	raw = &pcx->data;

	pcx_width = pcx->xmax - pcx->xmin;
	pcx_height = pcx->ymax - pcx->ymin;

	if ((pcx->manufacturer != 0x0a) || (pcx->version != 5) ||
		(pcx->encoding != 1) || (pcx->bits_per_pixel != 8) ||
		(pcx_width >= 4096) || (pcx_height >= 4096))
	{
		Com_Printf("Bad pcx file %s\n", filename);
		FS_FreeFile(pcx);
		return;
	}

	full_size = (pcx_height + 1) * (pcx_width + 1);
	out = malloc(full_size);
	if (!out)
	{
		Com_Printf("Can't allocate\n");
		FS_FreeFile(pcx);
		return;
	}

	*pic = out;

	pix = out;

	if (palette)
	{
		*palette = malloc(768);
		if (!(*palette))
		{
			Com_Printf("Can't allocate\n");
			free(out);
			FS_FreeFile(pcx);
			return;
		}
		if (len > 768)
		{
			memcpy(*palette, (byte *)pcx + len - 768, 768);
		}
		else
		{
			image_issues = true;
		}
	}

	if (width)
	{
		*width = pcx_width + 1;
	}

	if (height)
	{
		*height = pcx_height + 1;
	}

	for (y = 0; y <= pcx_height; y++, pix += pcx_width + 1)
	{
		for (x = 0; x <= pcx_width; )
		{
			if (raw - (byte *)pcx > len)
			{
				// no place for read
				image_issues = true;
				x = pcx_width;
				break;
			}
			dataByte = *raw++;

			if ((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				if (raw - (byte *)pcx > len)
				{
					// no place for read
					image_issues = true;
					x = pcx_width;
					break;
				}
				dataByte = *raw++;
			}
			else
			{
				runLength = 1;
			}

			while (runLength-- > 0)
			{
				if ((*pic + full_size) <= (pix + x))
				{
					// no place for write
					image_issues = true;
					x += runLength;
					runLength = 0;
				}
				else
				{
					pix[x++] = dataByte;
				}
			}
		}
	}

	if (raw - (byte *)pcx > len)
	{
		Com_DPrintf("PCX file %s was malformed", filename);
		free(*pic);
		*pic = NULL;
	}

	if (image_issues)
	{
		Com_Printf("PCX file %s has possible size issues.\n", filename);
	}

	FS_FreeFile(pcx);
}


/*
===============
R_FindImage

Finds or loads the given image
===============
*/
image_t	*R_FindImage (char *name, imagetype_t type)
{
	image_t	*image;
	int		i, len;
	byte	*pic, *palette;
	int		width, height;
#ifndef _WIN32
	char *ptr;
#endif

	if (!name)
		return NULL; // Com_Error (ERR_DROP, "R_FindImage: NULL name");
	len = strlen(name);
	if (len<5)
		return NULL; // Com_Error (ERR_DROP, "R_FindImage: bad name: %s", name);

#ifndef _WIN32
	// fix backslashes
	while ((ptr=strchr(name,'\\'))) {
	  *ptr = '/';
	}
#endif

	// look for it
	for (i=0, image=r_images ; i<numr_images ; i++,image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	//
	// load the pic from disk
	//
	pic = NULL;
	palette = NULL;
	if (!strcmp(name+len-4, ".pcx"))
	{
		SW_LoadPCX (name, &pic, &palette, &width, &height);
		if (!pic)
			return NULL; // Com_Error (ERR_DROP, "R_FindImage: can't load %s", name);
		image = R_LoadPic (name, pic, width, height, type);
	}
	else if (!strcmp(name+len-4, ".wal"))
	{
		image = R_LoadWal (name);
	}
	else if (!strcmp(name+len-4, ".tga"))
		return NULL; // Com_Error (ERR_DROP, "R_FindImage: can't load %s in software renderer", name);
	else
		return NULL; // Com_Error (ERR_DROP, "R_FindImage: bad extension on: %s", name);

	if (pic)
		free(pic);
	if (palette)
		free(palette);

	return image;
}



/*
===============
RE_RegisterSkin
===============
*/
struct image_s *RE_SW_RegisterSkin (char *name)
{
	return R_FindImage (name, it_skin);
}


/*
================
R_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void R_FreeUnusedImages (void)
{
	int		i;
	image_t	*image;

	for (i=0, image=r_images ; i<numr_images ; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
		{
			Com_PageInMemory ((byte *)image->pixels[0], image->width*image->height);
			continue; // used this sequence
		}
		if (!image->registration_sequence)
			continue; // free texture
		if (image->type == it_pic)
			continue; // don't free pics
		// free it
		free (image->pixels[0]); // the other mip levels just follow
		memset (image, 0, sizeof(*image));
	}
}



/*
===============
R_InitImages
===============
*/
void	R_InitImages (void)
{
	registration_sequence = 1;
}

/*
===============
R_ShutdownImages
===============
*/
void	R_ShutdownImages (void)
{
	int		i;
	image_t	*image;

	for (i=0, image=r_images ; i<numr_images ; i++, image++)
	{
		if (!image->registration_sequence)
			continue; // free texture
		// free it
		free (image->pixels[0]); // the other mip levels just follow
		memset (image, 0, sizeof(*image));
	}
}
