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

#include "config.h"

#include "jinete/jinete.h"

#include "commands/commands.h"
#include "modules/gui.h"
#include "modules/sprites.h"
#include "raster/cel.h"
#include "raster/layer.h"
#include "raster/sprite.h"
#include "raster/undo.h"
#include "util/functions.h"

static bool cmd_remove_frame_enabled(const char *argument)
{
  return
    current_sprite != NULL &&
    current_sprite->frames > 1;
}

static void cmd_remove_frame_execute(const char *argument)
{
  Sprite *sprite = current_sprite;

  if (undo_is_enabled(sprite->undo))
    undo_set_label(sprite->undo, "Remove Frame");

  RemoveFrame(sprite, sprite->frame);

  update_screen_for_sprite(current_sprite);
}

Command cmd_remove_frame = {
  CMD_REMOVE_FRAME,
  cmd_remove_frame_enabled,
  NULL,
  cmd_remove_frame_execute,
  NULL
};