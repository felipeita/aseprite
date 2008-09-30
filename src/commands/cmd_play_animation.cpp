/* ASE - Allegro Sprite Editor
 * Copyright (C) 2008  David A. Capello
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

#include <allegro.h>

#include "jinete/jinete.h"

#include "commands/commands.h"
#include "modules/editors.h"
#include "modules/gui.h"
#include "modules/palettes.h"
#include "modules/sprites.h"
#include "modules/tools.h"
#include "raster/palette.h"
#include "raster/sprite.h"
#include "widgets/editor.h"

static int speed_timer;

static void speed_timer_callback(void)
{
  speed_timer++;
}

END_OF_STATIC_FUNCTION(speed_timer_callback);

static bool cmd_play_animation_enabled(const char *argument)
{
  return current_sprite != NULL;
}

static void cmd_play_animation_execute(const char *argument)
{
  Sprite *sprite = current_sprite;
  int old_frame, msecs;
  bool done = FALSE;
  bool onionskin = get_onionskin();
  Palette *oldpal, *newpal;
  PALETTE rgbpal;

  if (sprite->frames < 2)
    return;

  /* desactivate the onion-skin */
  set_onionskin(FALSE);

  jmouse_hide();

  old_frame = sprite->frame;

  LOCK_VARIABLE(speed_timer);
  LOCK_FUNCTION(speed_timer_callback);

  clear_keybuf();

  /* clear all the screen */
  clear_bitmap(ji_screen);

  /* do animation */
  oldpal = NULL;
  speed_timer = 0;
  while (!done) {
    msecs = sprite_get_frlen(sprite, sprite->frame);
    install_int_ex(speed_timer_callback, MSEC_TO_TIMER(msecs));

    newpal = sprite_get_palette(sprite, sprite->frame);
    if (oldpal != newpal) {
      set_palette(palette_to_allegro(newpal, rgbpal));
      oldpal = newpal;
    }

    editor_draw_sprite_safe(current_editor, 0, 0, sprite->w, sprite->h);

    do {
      poll_mouse();
      poll_keyboard();
      if (keypressed() || mouse_b)
	done = TRUE;
      gui_feedback();
    } while (!done && (speed_timer <= 0));

    if (!done) {
      sprite->frame++;
      if (sprite->frame >= sprite->frames)
	sprite->frame = 0;

      speed_timer--;
    }
    gui_feedback();
  }

  set_onionskin(onionskin);

  /* if right-click or ESC */
  if (mouse_b == 2 || (keypressed() && (readkey()>>8) == KEY_ESC))
    /* return to the old frame position */
    sprite->frame = old_frame;

  /* refresh all */
  newpal = sprite_get_palette(sprite, sprite->frame);
  set_current_palette(newpal, TRUE);
  jmanager_refresh_screen();
  gui_feedback();

  while (mouse_b)
    poll_mouse();

  clear_keybuf();
  remove_int(speed_timer_callback);

  jmouse_show();
}

Command cmd_play_animation = {
  CMD_PLAY_ANIMATION,
  cmd_play_animation_enabled,
  NULL,
  cmd_play_animation_execute,
};