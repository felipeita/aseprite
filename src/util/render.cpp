/* ASE - Allegro Sprite Editor
 * Copyright (C) 2001-2008  David A. Capello
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* TODO modificable option: scalable-tiles */
#define SCALABLE_TILES

#include "config.h"

#include <assert.h>

#include "jinete/jlist.h"

#include "modules/palettes.h"
#include "modules/tools.h"
#include "raster/raster.h"

/************************************************************************/
/* Render engine */

static int global_opacity = 255;

static Layer *selected_layer = NULL;
static Image *rastering_image = NULL;

static void render_layer(Sprite *sprite, Layer *layer, Image *image,
			 int source_x, int source_y,
			 int frame, int zoom,
			 void (*zoomed_func)(Image *, Image *, int, int, int, int, int),
			 bool render_background,
			 bool render_transparent);

static void merge_zoomed_image8(Image *dst, Image *src, int x, int y, int opacity, int blend_mode, int zoom);
static void merge_zoomed_image16(Image *dst, Image *src, int x, int y, int opacity, int blend_mode, int zoom);
static void merge_zoomed_image32(Image *dst, Image *src, int x, int y, int opacity, int blend_mode, int zoom);

void set_preview_image(Layer *layer, Image *image)
{
  selected_layer = layer;
  rastering_image = image;
}

/**
 * Draws the @a frame animation frame of the @a source image in the
 * return image, all positions must have the zoom applied
 * (sorce_x<<zoom, dest_x<<zoom, width<<zoom, etc.)
 *
 * This routine is used to render the sprite 
 */
Image *render_sprite(Sprite *sprite,
		     int source_x, int source_y,
		     int width, int height,
		     int frame, int zoom)
{
  void (*zoomed_func)(Image *, Image *, int, int, int, int, int);
  Layer *background = sprite_get_background_layer(sprite);
  bool need_grid = (background != NULL ? !layer_is_readable(background): TRUE);
  int depth;
  Image *image;

  switch (sprite->imgtype) {

    case IMAGE_RGB:
      depth = 32;
      zoomed_func = merge_zoomed_image32;
      break;

    case IMAGE_GRAYSCALE:
      depth = 8;
      zoomed_func = merge_zoomed_image16;
      break;

    case IMAGE_INDEXED:
      depth = 8;
      zoomed_func = merge_zoomed_image8;
      break;

    default:
      return NULL;
  }

  /* create a temporary bitmap to draw all to it */
  image = image_new(sprite->imgtype, width, height);
  if (!image)
    return NULL;

  /* draw the background */
  if (need_grid) {
    int x, y, u, v, c1, c2;

    switch (image->imgtype) {
      case IMAGE_RGB:
	c1 = _rgba(128, 128, 128, 255); /* TODO configurable grid color */
	c2 = _rgba(192, 192, 192, 255);
        break;
      case IMAGE_GRAYSCALE:
	c1 = _graya(128, 255);
	c2 = _graya(192, 255);
        break;
	/* TODO remove this */
      /* case IMAGE_INDEXED: */
      /* 	c1 = rgb_map->data[16][16][16]; */
      /*   c2 = rgb_map->data[24][24][24]; */
      /*   break; */
      default:
	c1 = c2 = 0;
	break;
    }

#ifdef SCALABLE_TILES
    u = (-source_x / (16<<zoom)) * (16<<zoom);
    v = (-source_y / (16<<zoom)) * (16<<zoom);
    for (y=-source_y; y<height+(16<<zoom); y+=(16<<zoom)) {
      for (x=-source_x; x<width+(16<<zoom); x+=(16<<zoom))
        image_rectfill(image,
		       x,
		       y,
		       x+(16<<zoom)-1,
		       y+(16<<zoom)-1,
		       ((u++)&1)? c1: c2);
      u = (++v);
    }
#else
    u = (-source_x / (16<<zoom)) * 16;
    v = (-source_y / (16<<zoom)) * 16;
    for (y=-source_y; y<height+16; y+=16) {
      for (x=-source_x; x<width+16; x+=16)
        image_rectfill(image,
		       x,
		       y,
		       x+16-1,
		       y+16-1,
		       ((u++)&1)? c1: c2);
      u = (++v);
    }
#endif
  }
  else
    image_clear(image, 0);

  color_map = NULL;

  /* onion-skin feature: draw the previous frame */
  if (get_onionskin() && (frame > 0)) {
    /* draw background layer of the current frame with opacity=255 */
    color_map = NULL;
    global_opacity = 255;

    render_layer(sprite, sprite->set, image, source_x, source_y,
		 frame, zoom, zoomed_func, TRUE, FALSE);

    /* draw transparent layers of the previous frame with opacity=128 */
    color_map = orig_trans_map;
    global_opacity = 128;

    render_layer(sprite, sprite->set, image, source_x, source_y,
		 frame-1, zoom, zoomed_func, FALSE, TRUE);

    /* draw transparent layers of the current frame with opacity=255 */
    color_map = NULL;
    global_opacity = 255;

    render_layer(sprite, sprite->set, image, source_x, source_y,
		 frame, zoom, zoomed_func, FALSE, TRUE);
  }
  /* just draw the current frame */
  else {
    render_layer(sprite, sprite->set, image, source_x, source_y,
		 frame, zoom, zoomed_func, TRUE, TRUE);
  }

  return image;
}

static void render_layer(Sprite *sprite, Layer *layer, Image *image,
			 int source_x, int source_y,
			 int frame, int zoom,
			 void (*zoomed_func)(Image *, Image *, int, int, int, int, int),
			 bool render_background,
			 bool render_transparent)
{
  /* we can't read from this layer */
  if (!layer_is_readable(layer))
    return;

  switch (layer->gfxobj.type) {

    case GFXOBJ_LAYER_IMAGE: {
      Image *src_image;
      Cel *cel;

      if ((!render_background  &&  layer_is_background(layer)) ||
	  (!render_transparent && !layer_is_background(layer)))
	break;

      cel = layer_get_cel(layer, frame);      
      if (cel != NULL) {
	/* is the 'rastering_image' setted to be used with this layer? */
	if ((frame == sprite->frame) &&
	    (selected_layer == layer) &&
	    (rastering_image != NULL)) {
	  src_image = rastering_image;
	}
	/* if not, we use the original cel-image from the images' stock */
	else if ((cel->image >= 0) &&
		 (cel->image < layer->sprite->stock->nimage))
	  src_image = layer->sprite->stock->image[cel->image];
	else
	  src_image = NULL;

	if (src_image) {
	  int output_opacity;
	  register int t;

	  output_opacity = MID(0, cel->opacity, 255);
	  output_opacity = INT_MULT(output_opacity, global_opacity, t);

	  if (zoom == 0) {
	    image_merge(image, src_image,
			cel->x - source_x,
			cel->y - source_y,
			output_opacity, layer->blend_mode);
	  }
	  else {
	    (*zoomed_func)(image, src_image,
			   (cel->x << zoom) - source_x,
			   (cel->y << zoom) - source_y,
			   output_opacity, layer->blend_mode, zoom);
	  }
	}
      }
      break;
    }

    case GFXOBJ_LAYER_SET: {
      JLink link;
      JI_LIST_FOR_EACH(layer->layers, link)
	render_layer(sprite, reinterpret_cast<Layer*>(link->data), image,
		     source_x, source_y,
		     frame, zoom, zoomed_func,
		     render_background,
		     render_transparent);
      break;
    }

  }
}

static void merge_zoomed_image8(Image *dst, Image *src,
				int x, int y, int opacity,
				int blend_mode, int zoom)
{
  ase_uint8 *src_address;
  ase_uint8 *dst_address;
  ase_uint8 *scanline, *scanline_address;
  int src_x, src_y, src_w, src_h;
  int dst_x, dst_y, dst_w, dst_h;
  int box_x, box_y, box_w, box_h;
  int sizeof_box, offsetx, offsety;
  int line_x, line_h, right, bottom;

  box_w = 1<<zoom;
  box_h = 1<<zoom;

  src_x = 0;
  src_y = 0;
  src_w = src->w;
  src_h = src->h;

  dst_x = x;
  dst_y = y;
  dst_w = src->w<<zoom;
  dst_h = src->h<<zoom;

  if (dst_x < 0) {
    src_x += (-dst_x)>>zoom;
    src_w -= (-dst_x)>>zoom;
    dst_w -= (-dst_x);
    offsetx = box_w - ((-dst_x) % box_w);
    dst_x = 0;
  }
  else
    offsetx = 0;

  if (dst_y < 0) {
    src_y += (-dst_y)>>zoom;
    src_h -= (-dst_y)>>zoom;
    dst_h -= (-dst_y);
    offsety = box_h - ((-dst_y) % box_h);
    dst_y = 0;
  }
  else
    offsety = 0;

  if (dst_x+dst_w > dst->w) {
    src_w -= (dst_x+dst_w-dst->w) >> zoom;
    dst_w = dst->w - dst_x;
  }

  if (dst_y+dst_h > dst->h) {
    src_h -= (dst_y+dst_h-dst->h) >> zoom;
    dst_h = dst->h - dst_y;
  }

  if ((src_w <= 0) || (src_h <= 0) || (dst_w <= 0) || (dst_h <= 0))
    return;

  sizeof_box = sizeof(ase_uint8) * box_w;
  right = dst_x+dst_w-1;
  bottom = dst_y+dst_h-1;

  scanline = (ase_uint8*)jmalloc(sizeof(ase_uint8) * src_w);

  /* merge process */

  for (y=0; y<src_h; y++) {
    /* process a new line */
    src_address = ((ase_uint8 **)src->line)[src_y] + src_x;
    dst_address = ((ase_uint8 **)dst->line)[dst_y] + dst_x;
    scanline_address = scanline;

    /* read `src' and `dst' and blend them, put the result in `scanline' */
    for (x=0; x<src_w; x++) {
      if (blend_mode == BLEND_MODE_COPY) {
	*scanline_address = *src_address;
      }
      else {
	if (*src_address) {
	  if (color_map)
	    *scanline_address = color_map->data[*src_address][*dst_address];
	  else
	    *scanline_address = *src_address;
	}
	else
	    *scanline_address = *dst_address;
      }

      src_address++;
      dst_address += box_w;
      scanline_address++;
    }

    /* get the `height' of the line */
    if ((offsety > 0) && (y == 0))
      line_h = offsety;
    else
      line_h = box_h;

    /* put the line in `dst' */
    for (box_y=0; box_y<line_h; box_y++) {
      dst_address = ((ase_uint8 **)dst->line)[dst_y] + dst_x;
      scanline_address = scanline;

      line_x = dst_x;
      x = 0;

      /* first pixel */
      if (offsetx > 0) {
        for (box_x=0; box_x<offsetx; box_x++) {
          (*dst_address++) = (*scanline_address);

          line_x++;
          if (line_x > right)
            goto done_with_line;
        }

        scanline_address++;
        x++;
      }

      /* the rest of the line */
      for (; x<src_w; x++) {
        for (box_x=0; box_x<box_w; box_x++) {
          (*dst_address++) = (*scanline_address);

          line_x++;
          if (line_x > right)
            goto done_with_line;
        }

        scanline_address++;
      }

done_with_line:;

      dst_y++;
      if (dst_y > bottom)
        goto done_with_blit;
    }

    /* go to the next line */
    src_y++;
  }

done_with_blit:;
  jfree(scanline);
}

static void merge_zoomed_image16(Image *dst, Image *src,
				 int x, int y, int opacity,
				 int blend_mode, int zoom)
{
  BLEND_COLOR blender;
  ase_uint16 *src_address;
  ase_uint16 *dst_address;
  ase_uint16 *scanline, *scanline_address;
  int src_x, src_y, src_w, src_h;
  int dst_x, dst_y, dst_w, dst_h;
  int box_x, box_y, box_w, box_h;
  int sizeof_box, offsetx, offsety;
  int line_x, line_h, right, bottom;

  assert(blend_mode >= 0 && blend_mode < BLEND_MODE_MAX);

  blender = _graya_blenders[blend_mode];

  box_w = 1<<zoom;
  box_h = 1<<zoom;

  src_x = 0;
  src_y = 0;
  src_w = src->w;
  src_h = src->h;

  dst_x = x;
  dst_y = y;
  dst_w = src->w<<zoom;
  dst_h = src->h<<zoom;

  if (dst_x < 0) {
    src_x += (-dst_x)>>zoom;
    src_w -= (-dst_x)>>zoom;
    dst_w -= (-dst_x);
    offsetx = box_w - ((-dst_x) % box_w);
    dst_x = 0;
  }
  else
    offsetx = 0;

  if (dst_y < 0) {
    src_y += (-dst_y)>>zoom;
    src_h -= (-dst_y)>>zoom;
    dst_h -= (-dst_y);
    offsety = box_h - ((-dst_y) % box_h);
    dst_y = 0;
  }
  else
    offsety = 0;

  if (dst_x+dst_w > dst->w) {
    src_w -= (dst_x+dst_w-dst->w) >> zoom;
    dst_w = dst->w - dst_x;
  }

  if (dst_y+dst_h > dst->h) {
    src_h -= (dst_y+dst_h-dst->h) >> zoom;
    dst_h = dst->h - dst_y;
  }

  if ((src_w <= 0) || (src_h <= 0) || (dst_w <= 0) || (dst_h <= 0))
    return;

  sizeof_box = sizeof(ase_uint16) * box_w;
  right = dst_x+dst_w-1;
  bottom = dst_y+dst_h-1;

  scanline = (ase_uint16*)jmalloc(sizeof(ase_uint16) * src_w);

  /* merge process */

  /* opacity = (opacity)? opacity+1: 0; */

  for (y=0; y<src_h; y++) {
    /* process a new line */
    src_address = ((ase_uint16 **)src->line)[src_y] + src_x;
    dst_address = ((ase_uint16 **)dst->line)[dst_y] + dst_x;
    scanline_address = scanline;

    /* read `src' and `dst' and blend them, put the result in `scanline' */
    for (x=0; x<src_w; x++) {
      *scanline_address = (*blender)(*dst_address, *src_address, opacity);

      src_address++;
      dst_address += box_w;
      scanline_address++;
    }

    /* get the `height' of the line */
    if ((offsety > 0) && (y == 0))
      line_h = offsety;
    else
      line_h = box_h;

    /* put the line in `dst' */
    for (box_y=0; box_y<line_h; box_y++) {
      dst_address = ((ase_uint16 **)dst->line)[dst_y] + dst_x;
      scanline_address = scanline;

      line_x = dst_x;
      x = 0;

      /* first pixel */
      if (offsetx > 0) {
        for (box_x=0; box_x<offsetx; box_x++) {
          (*dst_address++) = (*scanline_address);

          line_x++;
          if (line_x > right)
            goto done_with_line;
        }

        scanline_address++;
        x++;
      }

      /* the rest of the line */
      for (; x<src_w; x++) {
        for (box_x=0; box_x<box_w; box_x++) {
          (*dst_address++) = (*scanline_address);

          line_x++;
          if (line_x > right)
            goto done_with_line;
        }

        scanline_address++;
      }

done_with_line:;

      dst_y++;
      if (dst_y > bottom)
        goto done_with_blit;
    }

    /* go to the next line */
    src_y++;
  }

done_with_blit:;
  jfree(scanline);
}

static void merge_zoomed_image32(Image *dst, Image *src,
				 int x, int y, int opacity,
				 int blend_mode, int zoom)
{
  BLEND_COLOR blender;
  ase_uint32 *src_address;
  ase_uint32 *dst_address;
  ase_uint32 *scanline, *scanline_address;
  int src_x, src_y, src_w, src_h;
  int dst_x, dst_y, dst_w, dst_h;
  int box_x, box_y, box_w, box_h;
  int sizeof_box, offsetx, offsety;
  int line_x, line_h, right, bottom;

  assert(blend_mode >= 0 && blend_mode < BLEND_MODE_MAX);

  blender = _rgba_blenders[blend_mode];

  box_w = 1<<zoom;
  box_h = 1<<zoom;

  src_x = 0;
  src_y = 0;
  src_w = src->w;
  src_h = src->h;

  dst_x = x;
  dst_y = y;
  dst_w = src->w<<zoom;
  dst_h = src->h<<zoom;

  if (dst_x < 0) {
    src_x += (-dst_x)>>zoom;
    src_w -= (-dst_x)>>zoom;
    dst_w -= (-dst_x);
    offsetx = box_w - ((-dst_x) % box_w);
    dst_x = 0;
  }
  else
    offsetx = 0;

  if (dst_y < 0) {
    src_y += (-dst_y)>>zoom;
    src_h -= (-dst_y)>>zoom;
    dst_h -= (-dst_y);
    offsety = box_h - ((-dst_y) % box_h);
    dst_y = 0;
  }
  else
    offsety = 0;

  if (dst_x+dst_w > dst->w) {
    src_w -= (dst_x+dst_w-dst->w) >> zoom;
    dst_w = dst->w - dst_x;
  }

  if (dst_y+dst_h > dst->h) {
    src_h -= (dst_y+dst_h-dst->h) >> zoom;
    dst_h = dst->h - dst_y;
  }

  if ((src_w <= 0) || (src_h <= 0) || (dst_w <= 0) || (dst_h <= 0))
    return;

  sizeof_box = sizeof(ase_uint32) * box_w;
  right = dst_x+dst_w-1;
  bottom = dst_y+dst_h-1;

  scanline = (ase_uint32*)jmalloc(sizeof(ase_uint32) * src_w);

  /* merge process */

  /* opacity = (opacity)? opacity+1: 0; */

  for (y=0; y<src_h; y++) {
    /* process a new line */
    src_address = ((ase_uint32 **)src->line)[src_y] + src_x;
    dst_address = ((ase_uint32 **)dst->line)[dst_y] + dst_x;
    scanline_address = scanline;

    /* read `src' and `dst' and blend them, put the result in `scanline' */
    for (x=0; x<src_w; x++) {
      *scanline_address = (*blender)(*dst_address, *src_address, opacity);

      src_address++;
      dst_address += box_w;
      scanline_address++;
    }

    /* get the `height' of the line */
    if ((offsety > 0) && (y == 0))
      line_h = offsety;
    else
      line_h = box_h;

    /* put the line in `dst' */
    for (box_y=0; box_y<line_h; box_y++) {
      dst_address = ((ase_uint32 **)dst->line)[dst_y] + dst_x;
      scanline_address = scanline;

      line_x = dst_x;
      x = 0;

      /* first pixel */
      if (offsetx > 0) {
        for (box_x=0; box_x<offsetx; box_x++) {
          (*dst_address++) = (*scanline_address);

          line_x++;
          if (line_x > right)
            goto done_with_line;
        }

        scanline_address++;
        x++;
      }

      /* the rest of the line */
      for (; x<src_w; x++) {
        for (box_x=0; box_x<box_w; box_x++) {
          (*dst_address++) = (*scanline_address);

          line_x++;
          if (line_x > right)
            goto done_with_line;
        }

        scanline_address++;
      }

done_with_line:;

      dst_y++;
      if (dst_y > bottom)
        goto done_with_blit;
    }

    /* go to the next line */
    src_y++;
  }

done_with_blit:;
  jfree(scanline);
}