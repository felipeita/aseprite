/* Jinete - a GUI library
 * Copyright (C) 2003-2008 David A. Capello.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of the Jinete nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* #define REPORT_SIGNALS */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#ifdef REPORT_SIGNALS
#  include <stdio.h>
#endif
#include <allegro.h>
#ifdef ALLEGRO_WINDOWS
#include <winalleg.h>
#endif

#include "jinete/jinete.h"
#include "jinete/jintern.h"

static bool widget_msg_proc(JWidget widget, JMessage msg);

int ji_register_widget_type(void)
{
  static int type = JI_USER_WIDGET;
  return type++;
}

/* creates a new widget with an unique JID */
JWidget jwidget_new(int type)
{
  JWidget widget = _ji_get_new_widget();
  if (!widget)
    return NULL;

  widget->type = type;
  widget->name = NULL;
  widget->rc = jrect_new(0, 0, 0, 0);
  widget->border_width.l = 0;
  widget->border_width.t = 0;
  widget->border_width.r = 0;
  widget->border_width.b = 0;
  widget->child_spacing = 0;
  widget->flags = 0;
  widget->emit_signals = 0;
  widget->min_w = 0;
  widget->min_h = 0;
  widget->max_w = INT_MAX;
  widget->max_h = INT_MAX;
  widget->children = jlist_new();
  widget->parent = NULL;
  widget->theme = ji_get_theme();
  widget->hooks = jlist_new();
  widget->draw_type = type;
  widget->draw_method = NULL;

  widget->align = 0;
  widget->text_size = 0;
  widget->text = NULL;
  widget->text_size_pix = 0;
  widget->text_font = widget->theme ? widget->theme->default_font: NULL;
  widget->bg_color = -1;

  widget->update_region = jregion_new(NULL, 0);

  widget->theme_data[0] = NULL;
  widget->theme_data[1] = NULL;
  widget->theme_data[2] = NULL;
  widget->theme_data[3] = NULL;

  widget->user_data[0] = NULL;
  widget->user_data[1] = NULL;
  widget->user_data[2] = NULL;
  widget->user_data[3] = NULL;

  jwidget_add_hook(widget, JI_WIDGET, widget_msg_proc, NULL);

  return widget;
}

void jwidget_free(JWidget widget)
{
  JLink link, next;
  JMessage msg;

  assert_valid_widget(widget);

  /* send destroy message */
  msg = jmessage_new(JM_DESTROY);
  jwidget_send_message(widget, msg);
  jmessage_free(msg);

  /* break relationship with the manager */
  jmanager_free_widget(widget);
  jmanager_remove_messages_for(widget);
  jmanager_remove_msg_filter_for(widget);

  /* remove from parent */
  if (widget->parent)
    jwidget_remove_child(widget->parent, widget);

  /* remove children */
  JI_LIST_FOR_EACH_SAFE(widget->children, link, next)
    jwidget_free(reinterpret_cast<JWidget>(link->data));
  jlist_free(widget->children);

  /* destroy the update region */
  if (widget->update_region)
    jregion_free(widget->update_region);

  /* destroy the text */
  if (widget->text)
    jfree(widget->text);

  /* destroy the name */
  if (widget->name)
    jfree(widget->name);

  /* destroy widget position */
  if (widget->rc)
    jrect_free(widget->rc);

  /* destroy hooks */
  JI_LIST_FOR_EACH(widget->hooks, link)
    jhook_free(reinterpret_cast<JHook>(link->data));
  jlist_free(widget->hooks);

  /* low level free */
  _ji_free_widget(widget);
}

void jwidget_free_deferred(JWidget widget)
{
  JMessage msg;

  assert_valid_widget(widget);

  msg = jmessage_new(JM_DEFERREDFREE);
  msg->deffree.widget_to_free = widget;
  /* TODO use the manager of 'widget' */
  jmessage_add_dest(msg, ji_get_default_manager());
  jmanager_enqueue_message(msg);
}

void jwidget_init_theme(JWidget widget)
{
  assert_valid_widget(widget);

  if (widget->theme) {
    if (!widget->draw_method)
      widget->draw_method = jtheme_get_method(widget->theme, widget->draw_type);

    if (widget->theme->init_widget) {
      (*widget->theme->init_widget)(widget);

      if (!(widget->flags & JI_INITIALIZED))
	widget->flags |= JI_INITIALIZED;

      jwidget_emit_signal(widget, JI_SIGNAL_INIT_THEME);
    }
  }
}

/**********************************************************************/
/* HOOKS */

/**
 * Adds a new hook for the widget.
 *
 * @see jhook
 */
void jwidget_add_hook(JWidget widget, int type,
		      JMessageFunc msg_proc, void *data)
{
  JHook hook;

  assert_valid_widget(widget);

  hook = jhook_new();
  hook->type = type;
  hook->msg_proc = msg_proc;
  hook->data = data;

  jlist_prepend(widget->hooks, hook);
}

/**
 * Returns the hook of the specified type.
 */
JHook jwidget_get_hook(JWidget widget, int type)
{
  JLink link;
  assert_valid_widget(widget);

  JI_LIST_FOR_EACH(widget->hooks, link) {
    if (((JHook)link->data)->type == type)
      return ((JHook)link->data);
  }
  return NULL;
}

/**
 * Returns the data associated to the specified hook.
 */
void *jwidget_get_data(JWidget widget, int type)
{
  register JLink link;
  assert_valid_widget(widget);

  JI_LIST_FOR_EACH(widget->hooks, link) {
    if (((JHook)link->data)->type == type)
      return ((JHook)link->data)->data;
  }

  return NULL;
}

void _jwidget_add_hook(JWidget widget, JHook hook)
{
  assert_valid_widget(widget);

  jlist_prepend(widget->hooks, hook);
}

void _jwidget_remove_hook(JWidget widget, JHook hook)
{
  assert_valid_widget(widget);

  jlist_remove(widget->hooks, hook);
}

/**********************************************************************/
/* main properties */

int jwidget_get_type(JWidget widget)
{
  assert_valid_widget(widget);

  return widget->type;
}

const char *jwidget_get_name(JWidget widget)
{
  assert_valid_widget(widget);

  return widget->name;
}

const char *jwidget_get_text(JWidget widget)
{
  assert_valid_widget(widget);

  jwidget_emit_signal(widget, JI_SIGNAL_GET_TEXT);
  return widget->text;
}

int jwidget_get_align(JWidget widget)
{
  assert_valid_widget(widget);

  return widget->align;
}

FONT *jwidget_get_font(JWidget widget)
{
  assert_valid_widget(widget);

  return widget->text_font;
}

void jwidget_set_name(JWidget widget, const char *name)
{
  assert_valid_widget(widget);

  if (widget->name)
    jfree(widget->name);

  widget->name = name ? jstrdup(name) : NULL;
}

void jwidget_set_text(JWidget widget, const char *text)
{
  assert_valid_widget(widget);

  jwidget_set_text_soft(widget, text);

  jwidget_emit_signal(widget, JI_SIGNAL_SET_TEXT);
  jwidget_dirty(widget);
}

void jwidget_set_text_soft(JWidget widget, const char *text)
{
  assert_valid_widget(widget);

  if (text) {
    /* more space needed */
    if (!widget->text || widget->text_size < strlen(text)+1) {
      widget->text_size = strlen(text)+1;
      widget->text = (char*)jrealloc(widget->text, widget->text_size);
    }

    /* copy the text string */
    strcpy(widget->text, text);

    if (widget->text_font)
      widget->text_size_pix = ji_font_text_len(widget->text_font,
					       widget->text);
  }
  /* NULL text */
  else if (widget->text) {
    widget->text_size = 0;
    jfree(widget->text);
    widget->text = NULL;
  }
}

void jwidget_set_align(JWidget widget, int align)
{
  assert_valid_widget(widget);

  widget->align = align;

  jwidget_dirty(widget);
}

void jwidget_set_font(JWidget widget, FONT *font)
{
  assert_valid_widget(widget);

  widget->text_font = font;

  if (widget->text && widget->text_font)
    widget->text_size_pix = ji_font_text_len(widget->text_font,
					     widget->text);
  else
    widget->text_size_pix = 0;

  jwidget_emit_signal(widget, JI_SIGNAL_SET_FONT);
  jwidget_dirty(widget);
}

/**********************************************************************/
/* behavior properties */

void jwidget_magnetic(JWidget widget, bool state)
{
  assert_valid_widget(widget);

  if (state)
    widget->flags |= JI_MAGNETIC;
  else
    widget->flags &= ~JI_MAGNETIC;
}

void jwidget_expansive(JWidget widget, bool state)
{
  assert_valid_widget(widget);

  if (state)
    widget->flags |= JI_EXPANSIVE;
  else
    widget->flags &= ~JI_EXPANSIVE;
}

void jwidget_decorative(JWidget widget, bool state)
{
  assert_valid_widget(widget);

  if (state)
    widget->flags |= JI_DECORATIVE;
  else
    widget->flags &= ~JI_DECORATIVE;
}

void jwidget_focusrest(JWidget widget, bool state)
{
  assert_valid_widget(widget);

  if (state)
    widget->flags |= JI_FOCUSREST;
  else
    widget->flags &= ~JI_FOCUSREST;
}

bool jwidget_is_magnetic(JWidget widget)
{
  assert_valid_widget(widget);

  return (widget->flags & JI_MAGNETIC) ? TRUE: FALSE;
}

bool jwidget_is_expansive(JWidget widget)
{
  assert_valid_widget(widget);

  return (widget->flags & JI_EXPANSIVE) ? TRUE: FALSE;
}

bool jwidget_is_decorative(JWidget widget)
{
  assert_valid_widget(widget);

  return (widget->flags & JI_DECORATIVE) ? TRUE: FALSE;
}

bool jwidget_is_focusrest(JWidget widget)
{
  assert_valid_widget(widget);

  return (widget->flags & JI_FOCUSREST) ? TRUE: FALSE;
}

/**********************************************************************/
/* status properties */

void jwidget_dirty(JWidget widget)
{
  assert_valid_widget(widget);
  jwidget_invalidate(widget);
}

void jwidget_show(JWidget widget)
{
  assert_valid_widget(widget);

  if (widget->flags & JI_HIDDEN) {
    widget->flags &= ~JI_HIDDEN;

    jwidget_dirty(widget);
    jwidget_emit_signal(widget, JI_SIGNAL_SHOW);
  }
}

void jwidget_hide(JWidget widget)
{
  assert_valid_widget(widget);

  if (!(widget->flags & JI_HIDDEN)) {
    jmanager_free_widget(widget); /* free from mananger */

    widget->flags |= JI_HIDDEN;
    jwidget_emit_signal(widget, JI_SIGNAL_HIDE);
  }
}

void jwidget_enable(JWidget widget)
{
  assert_valid_widget(widget);

  if (widget->flags & JI_DISABLED) {
    widget->flags &= ~JI_DISABLED;
    jwidget_dirty(widget);

    jwidget_emit_signal(widget, JI_SIGNAL_ENABLE);
  }
}

void jwidget_disable(JWidget widget)
{
  assert_valid_widget(widget);

  if (!(widget->flags & JI_DISABLED)) {
    jmanager_free_widget(widget); /* free from the manager */

    widget->flags |= JI_DISABLED;
    jwidget_dirty(widget);

    jwidget_emit_signal(widget, JI_SIGNAL_DISABLE);
  }
}

void jwidget_select(JWidget widget)
{
  assert_valid_widget(widget);

  if (!(widget->flags & JI_SELECTED)) {
    widget->flags |= JI_SELECTED;
    jwidget_dirty(widget);

    jwidget_emit_signal(widget, JI_SIGNAL_SELECT);
  }
}

void jwidget_deselect(JWidget widget)
{
  assert_valid_widget(widget);

  if (widget->flags & JI_SELECTED) {
    widget->flags &= ~JI_SELECTED;
    jwidget_dirty(widget);

    jwidget_emit_signal(widget, JI_SIGNAL_DESELECT);
  }
}

bool jwidget_is_visible(JWidget widget)
{
  assert_valid_widget(widget);

  return !(jwidget_is_hidden(widget));
}

bool jwidget_is_hidden(JWidget widget)
{
  assert_valid_widget(widget);

  do {
    if (widget->flags & JI_HIDDEN)
      return TRUE;

    widget = widget->parent;
  } while (widget);

  return FALSE;
}

bool jwidget_is_enabled(JWidget widget)
{
  assert_valid_widget(widget);

  return !(jwidget_is_disabled(widget));
}

bool jwidget_is_disabled(JWidget widget)
{
  assert_valid_widget(widget);

  do {
    if (widget->flags & JI_DISABLED)
      return TRUE;

    widget = widget->parent;
  } while (widget);

  return FALSE;
}

bool jwidget_is_selected(JWidget widget)
{
  assert_valid_widget(widget);

  return (widget->flags & JI_SELECTED) ? TRUE: FALSE;
}

bool jwidget_is_deselected(JWidget widget)
{
  assert_valid_widget(widget);

  return !(jwidget_is_selected(widget));
}

/**********************************************************************/
/* properties with manager */

bool jwidget_has_focus(JWidget widget)
{
  assert_valid_widget(widget);

  return (widget->flags & JI_HASFOCUS) ? TRUE: FALSE;
}

bool jwidget_has_mouse(JWidget widget)
{
  assert_valid_widget(widget);

  return (widget->flags & JI_HASMOUSE) ? TRUE: FALSE;
}

bool jwidget_has_capture(JWidget widget)
{
  assert_valid_widget(widget);

  return (widget->flags & JI_HASCAPTURE) ? TRUE: FALSE;
}

/**********************************************************************/
/* children handle */

void jwidget_add_child(JWidget widget, JWidget child)
{
  assert_valid_widget(widget);
  assert_valid_widget(child);

  jlist_append(widget->children, child);
  child->parent = widget;

  jwidget_emit_signal(child, JI_SIGNAL_NEW_PARENT);
  jwidget_emit_signal(widget, JI_SIGNAL_ADD_CHILD);
}

void jwidget_add_children(JWidget widget, ...)
{
  JWidget child;
  va_list ap;

  assert_valid_widget(widget);

  va_start(ap, widget);

  while ((child=va_arg(ap, JWidget)) != NULL)
    jwidget_add_child(widget, child);

  va_end(ap);
}

void jwidget_remove_child(JWidget widget, JWidget child)
{
  assert_valid_widget(widget);
  assert_valid_widget(child);

  jlist_remove(widget->children, child);
  child->parent = NULL;

  jwidget_emit_signal(child, JI_SIGNAL_NEW_PARENT);
  jwidget_emit_signal(widget, JI_SIGNAL_REMOVE_CHILD);
}

void jwidget_replace_child(JWidget widget, JWidget old_child, JWidget new_child)
{
  JLink before;

  assert_valid_widget(widget);
  assert_valid_widget(old_child);
  assert_valid_widget(new_child);

  before = jlist_find(widget->children, old_child);
  if (!before)
    return;
  before = before->next;

  jwidget_remove_child(widget, old_child);

  jlist_insert_before(widget->children, before, new_child);
  new_child->parent = widget;

  jwidget_emit_signal(new_child, JI_SIGNAL_NEW_PARENT);
  jwidget_emit_signal(widget, JI_SIGNAL_ADD_CHILD);
}

/**********************************************************************/
/* parents and children */

/* gets the widget parent */
JWidget jwidget_get_parent(JWidget widget)
{
  assert_valid_widget(widget);

  return widget->parent;
}

/* get the parent window */
JWidget jwidget_get_window(JWidget widget)
{
  assert_valid_widget(widget);

  while (widget) {
    if (widget->type == JI_WINDOW)
      return widget;

    widget = widget->parent;
  }

  return NULL;
}

JWidget jwidget_get_manager(JWidget widget)
{
  assert_valid_widget(widget);

  while (widget) {
    if (widget->type == JI_MANAGER)
      return widget;

    widget = widget->parent;
  }

  return ji_get_default_manager();
}

/* returns a list of parents (you must free the list), if "ascendant"
   is TRUE the list is build from child to parents, else the list is
   from parent to children */
JList jwidget_get_parents(JWidget widget, bool ascendant)
{
  JList list = jlist_new();

  assert_valid_widget(widget);

  for (; widget; widget=widget->parent) {
    /* append parents in tail */
    if (ascendant)
      jlist_append(list, widget);
    /* append parents in head */
    else
      jlist_prepend(list, widget);
  }

  return list;
}

/* returns a list of children (you must free the list) */
JList jwidget_get_children(JWidget widget)
{
  assert_valid_widget(widget);

  return jlist_copy(widget->children);
}

JWidget jwidget_pick(JWidget widget, int x, int y)
{
  JWidget inside, picked = NULL;
  JLink link;

  assert_valid_widget(widget);

  if (!(widget->flags & JI_HIDDEN) &&   /* is visible */
      jrect_point_in(widget->rc, x, y)) { /* the point is inside the bounds */
    picked = widget;

    JI_LIST_FOR_EACH(widget->children, link) {
      inside = jwidget_pick(reinterpret_cast<JWidget>(link->data), x, y);
      if (inside) {
	picked = inside;
	break;
      }
    }
  }

  return picked;
}

bool jwidget_has_child(JWidget widget, JWidget child)
{
  assert_valid_widget(widget);
  assert_valid_widget(child);

  return jlist_find(widget->children, child) != widget->children->end ? TRUE: FALSE;
}

/**********************************************************************/
/* position and geometry */

void jwidget_request_size(JWidget widget, int *w, int *h)
{
  JMessage msg;

  assert_valid_widget(widget);

  msg = jmessage_new(JM_REQSIZE);
  jwidget_send_message(widget, msg);
  *w = MID(widget->min_w, msg->reqsize.w, widget->max_w);
  *h = MID(widget->min_h, msg->reqsize.h, widget->max_h);
  jmessage_free(msg);
}

void jwidget_relayout(JWidget widget)
{
  jwidget_set_rect(widget, widget->rc);
  jwidget_dirty(widget);
}

/* gets the position of the widget */
JRect jwidget_get_rect(JWidget widget)
{
  assert_valid_widget(widget);

  return jrect_new_copy(widget->rc);
}

/* gets the position for children of the widget */
JRect jwidget_get_child_rect(JWidget widget)
{
  assert_valid_widget(widget);

  return jrect_new(widget->rc->x1 + widget->border_width.l,
		   widget->rc->y1 + widget->border_width.t,
		   widget->rc->x2 - widget->border_width.r,
		   widget->rc->y2 - widget->border_width.b);
}

JRegion jwidget_get_region(JWidget widget)
{
  JRegion region;

  assert_valid_widget(widget);

  if ((widget->type == JI_WINDOW) && (widget->theme->get_window_mask))
    region = (*widget->theme->get_window_mask)(widget);
  else
    region = jregion_new(widget->rc, 1);

  return region;
}

/* gets the region to be able to draw in */
JRegion jwidget_get_drawable_region(JWidget widget, int flags)
{
  JWidget window, manager, view, child;
  JRegion region, reg1, reg2, reg3;
  JList windows_list;
  JLink link;
  JRect cpos;

  assert_valid_widget(widget);

  region = jwidget_get_region(widget);

  /* cut the top windows areas */
  if (flags & JI_GDR_CUTTOPWINDOWS) {
    window = jwidget_get_window(widget);
    manager = window ? jwidget_get_manager(window): NULL;

    while (manager) {
      windows_list = manager->children;
      link = jlist_find(windows_list, window);

      if (!jlist_empty(windows_list) &&
	  window != jlist_first(windows_list)->data &&
	  link != windows_list->end) {
	/* subtract the rectangles */
	for (link=link->prev; link != windows_list->end; link=link->prev) {
	  reg1 = jwidget_get_region(reinterpret_cast<JWidget>(link->data));
	  jregion_subtract(region, region, reg1);
	  jregion_free(reg1);
	}
      }

      window = jwidget_get_window(manager);
      manager = window ? jwidget_get_manager(window): NULL;
    }
  }

  /* clip the areas where are children */
  if (!(flags & JI_GDR_USECHILDAREA) && !jlist_empty(widget->children)) {
    cpos = jwidget_get_child_rect(widget);
    reg1 = jregion_new(NULL, 0);
    reg2 = jregion_new(cpos, 1);
    JI_LIST_FOR_EACH(widget->children, link) {
      child = reinterpret_cast<JWidget>(link->data);
      if (jwidget_is_visible(child)) {
	reg3 = jwidget_get_region(child);
	if (child->flags & JI_DECORATIVE) {
	  jregion_reset(reg1, widget->rc);
	  jregion_intersect(reg1, reg1, reg3);
	}
	else {
	  jregion_intersect(reg1, reg2, reg3);
	}
	jregion_subtract(region, region, reg1);
	jregion_free(reg3);
      }
    }
    jregion_free(reg1);
    jregion_free(reg2);
    jrect_free(cpos);
  }

  /* intersect with the parent area */
  if (!(widget->flags & JI_DECORATIVE)) {
    JWidget parent = widget->parent;

    reg1 = jregion_new(NULL, 0);

    while (parent) {
      cpos = jwidget_get_child_rect(parent);
      jregion_reset(reg1, cpos);
      jregion_intersect(region, region, reg1);
      jrect_free(cpos);

      parent = parent->parent;
    }

    jregion_free(reg1);
  }
  else {
    JWidget parent = widget->parent;

    if (parent) {
      cpos = jwidget_get_rect(parent);
      reg1 = jregion_new(cpos, 1);
      jregion_intersect(region, region, reg1);
      jregion_free(reg1);
      jrect_free(cpos);
    }
  }

  /* limit to the manager area */
  window = jwidget_get_window(widget);
  manager = window ? jwidget_get_manager(window): NULL;

  while (manager) {
    view = jwidget_get_view(manager);
    if (view)
      cpos = jview_get_viewport_position(view);
    else
      cpos = jwidget_get_child_rect(manager);
/*     if (!manager->parent) */
/*       cpos = jwidget_get_rect(manager); */
/*     else */
/*       cpos = jwidget_get_child_rect(manager->parent); */

    reg1 = jregion_new(cpos, 1);
    jregion_intersect(region, region, reg1);
    jregion_free(reg1);
    jrect_free(cpos);

    window = jwidget_get_window(manager);
    manager = window ? jwidget_get_manager(window): NULL;
  }

  /* return the region */
  return region;
}

int jwidget_get_bg_color(JWidget widget)
{
  assert_valid_widget(widget);

  if (widget->bg_color < 0 && widget->parent)
    return jwidget_get_bg_color(widget->parent);
  else
    return widget->bg_color;
}

JTheme jwidget_get_theme(JWidget widget)
{
  assert_valid_widget(widget);

  return widget->theme;
}

int jwidget_get_text_length(JWidget widget)
{
#if 1
  return ji_font_text_len(widget->text_font, widget->text);
#else  /* use cached text size */
  return widget->text_size_pix;
#endif
}

int jwidget_get_text_height(JWidget widget)
{
  assert_valid_widget(widget);

  return text_height(widget->text_font);
}

void jwidget_get_texticon_info(JWidget widget,
			       JRect box, JRect text, JRect icon,
			       int icon_align, int icon_w, int icon_h)
{
#define SETRECT(r)				\
  if (r) {					\
    r->x1 = r##_x;				\
    r->y1 = r##_y;				\
    r->x2 = r##_x+r##_w;			\
    r->y2 = r##_y+r##_h;			\
  }

  int box_x, box_y, box_w, box_h, icon_x, icon_y;
  int text_x, text_y, text_w, text_h;

  assert_valid_widget(widget);

  text_x = text_y = 0;

  /* size of the text */
  if (widget->text) {
    text_w = jwidget_get_text_length(widget);
    text_h = jwidget_get_text_height(widget);
  }
  else {
    text_w = text_h = 0;
  }

  /* box size */
  if (icon_align & JI_CENTER) {	  /* with the icon in the center */
    if (icon_align & JI_MIDDLE) { /* with the icon inside the text */
      box_w = MAX(icon_w, text_w);
      box_h = MAX(icon_h, text_h);
    }
    /* with the icon in the top or bottom */
    else {
      box_w = MAX(icon_w, text_w);
      box_h = icon_h + ((widget->text)? widget->child_spacing: 0) + text_h;
    }
  }
  /* with the icon in left or right that doesn't care by now */
  else {
    box_w = icon_w + ((widget->text)? widget->child_spacing: 0) + text_w;
    box_h = MAX(icon_h, text_h);
  }

  /* box position */
  if (widget->align & JI_RIGHT)
    box_x = widget->rc->x2 - box_w - widget->border_width.r;
  else if (widget->align & JI_CENTER)
    box_x = (widget->rc->x1+widget->rc->x2)/2 - box_w/2;
  else
    box_x = widget->rc->x1 + widget->border_width.l;

  if (widget->align & JI_BOTTOM)
    box_y = widget->rc->y2 - box_h - widget->border_width.b;
  else if (widget->align & JI_MIDDLE)
    box_y = (widget->rc->y1+widget->rc->y2)/2 - box_h/2;
  else
    box_y = widget->rc->y1 + widget->border_width.t;

  /* with text */
  if (widget->text) {
    /* text/icon X position */
    if (icon_align & JI_RIGHT) {
      text_x = box_x;
      icon_x = box_x + box_w - icon_w;
    }
    else if (icon_align & JI_CENTER) {
      text_x = box_x + box_w/2 - text_w/2;
      icon_x = box_x + box_w/2 - icon_w/2;
    }
    else {
      text_x = box_x + box_w - text_w;
      icon_x = box_x;
    }

    /* text Y position */
    if (icon_align & JI_BOTTOM) {
      text_y = box_y;
      icon_y = box_y + box_h - icon_h;
    }
    else if (icon_align & JI_MIDDLE) {
      text_y = box_y + box_h/2 - text_h/2;
      icon_y = box_y + box_h/2 - icon_h/2;
    }
    else {
      text_y = box_y + box_h - text_h;
      icon_y = box_y;
    }
  }
  /* without text */
  else {
    /* icon X/Y position */
    icon_x = box_x;
    icon_y = box_y;
  }

  SETRECT(box);
  SETRECT(text);
  SETRECT(icon);
}

void jwidget_noborders(JWidget widget)
{
  widget->border_width.l = 0;
  widget->border_width.t = 0;
  widget->border_width.r = 0;
  widget->border_width.b = 0;
  widget->child_spacing = 0;

  jwidget_dirty(widget);
}

void jwidget_set_border(JWidget widget, int l, int t, int r, int b)
{
  assert_valid_widget(widget);

  widget->border_width.l = l;
  widget->border_width.t = t;
  widget->border_width.r = r;
  widget->border_width.b = b;

  jwidget_dirty(widget);
}

void jwidget_set_rect(JWidget widget, JRect rect)
{
  JMessage msg;

  assert_valid_widget(widget);

  msg = jmessage_new(JM_SETPOS);
  jrect_copy(&msg->setpos.rect, rect);
  jwidget_send_message(widget, msg);
  jmessage_free(msg);
}

void jwidget_set_min_size(JWidget widget, int w, int h)
{
  assert_valid_widget(widget);

  widget->min_w = w;
  widget->min_h = h;
}

void jwidget_set_max_size(JWidget widget, int w, int h)
{
  assert_valid_widget(widget);

  widget->max_w = w;
  widget->max_h = h;
}

void jwidget_set_bg_color(JWidget widget, int color)
{
  assert_valid_widget(widget);

  widget->bg_color = color;
}

void jwidget_set_theme(JWidget widget, JTheme theme)
{
  assert_valid_widget(widget);

  widget->theme = theme;
  /* TODO mmhhh... maybe some JStyle in JWidget should be great */
  widget->text_font = widget->theme ? widget->theme->default_font: NULL;
}

/**********************************************************************/
/* drawing methods */

void jwidget_flush_redraw(JWidget widget)
{
  int c, nrects;
  JMessage msg;
  JLink link;
  JRect rc;

  assert_valid_widget(widget);

  nrects = JI_REGION_NUM_RECTS(widget->update_region);
  if (nrects > 0) {
    /* get areas to draw */
    JRegion region = jwidget_get_drawable_region(widget, JI_GDR_CUTTOPWINDOWS);
    jregion_intersect(widget->update_region,
		      widget->update_region, region);
    jregion_free(region);

    nrects = JI_REGION_NUM_RECTS(widget->update_region);

    /* draw the widget */
    for (c=0, rc=JI_REGION_RECTS(widget->update_region);
	 c<nrects;
	 c++, rc++) {
      /* create the draw message */
      msg = jmessage_new(JM_DRAW);
      msg->draw.count = nrects-1 - c;
      msg->draw.rect = *rc;
      jmessage_add_dest(msg, widget);

      /* enqueue the draw message */
      jmanager_enqueue_message(msg);
    }

    jregion_empty(widget->update_region);
  }

  JI_LIST_FOR_EACH(widget->children, link)
    jwidget_flush_redraw((JWidget)link->data);
}

void jwidget_redraw_region(JWidget widget, const JRegion region)
{
  assert_valid_widget(widget);

  if (jwidget_is_visible(widget)) {
#if 1
    JMessage msg = jmessage_new(JM_DRAWRGN);
    msg->drawrgn.region = region;
    jwidget_send_message(widget, msg);
    jmessage_free(msg);
#else
    jwidget_invalidate_region(widget, region);
#endif
  }
}

void jwidget_invalidate(JWidget widget)
{
  assert_valid_widget(widget);

  if (jwidget_is_visible(widget)) {
    JRegion reg1 = jwidget_get_drawable_region(widget, JI_GDR_CUTTOPWINDOWS);
    JLink link;

    jregion_copy(widget->update_region, reg1);
    jregion_free(reg1);

    JI_LIST_FOR_EACH(widget->children, link)
      jwidget_invalidate(reinterpret_cast<JWidget>(link->data));
  }
}

void jwidget_invalidate_rect(JWidget widget, const JRect rect)
{
  assert_valid_widget(widget);

  if (jwidget_is_visible(widget)) {
    JRegion reg1 = jregion_new(rect, 1);
    jwidget_invalidate_region(widget, reg1);
    jregion_free(reg1);
  }
}

void jwidget_invalidate_region(JWidget widget, const JRegion region)
{
  assert_valid_widget(widget);

  if (jwidget_is_visible(widget) &&
      jregion_rect_in(region, widget->rc) != JI_RGNOUT) {
    JRegion reg1 = jregion_new(NULL, 0);
    JRegion reg2 = jwidget_get_drawable_region(widget,
					       JI_GDR_CUTTOPWINDOWS);
    JLink link;

    jregion_union(reg1, widget->update_region, region);
    jregion_intersect(widget->update_region, reg1, reg2);
    jregion_free(reg2);

    jregion_subtract(reg1, region, widget->update_region);

    JI_LIST_FOR_EACH(widget->children, link)
      jwidget_invalidate_region(reinterpret_cast<JWidget>(link->data), reg1);

    jregion_free(reg1);
  }
}

void jwidget_scroll(JWidget widget, JRegion region, int dx, int dy)
{
  if (dx != 0 || dy != 0) {
    JRegion reg2 = jregion_new(NULL, 0);
    
    jregion_copy(reg2, region);
    jregion_translate(reg2, dx, dy);
    jregion_intersect(reg2, reg2, region);

    jregion_translate(reg2, -dx, -dy);

    jmouse_hide();
    ji_move_region(reg2, dx, dy);
    jmouse_show();

    jregion_translate(reg2, dx, dy);

    jregion_union(widget->update_region, widget->update_region, region);
    jregion_subtract(widget->update_region, widget->update_region, reg2);

    /* refresh the update_region */
    jwidget_flush_redraw(widget);
    jmanager_dispatch_messages(ji_get_default_manager());

    jregion_free(reg2);
  }
}

/**********************************************************************/
/* signal handle */

void jwidget_signal_on(JWidget widget)
{
  assert_valid_widget(widget);

  widget->emit_signals--;
}

void jwidget_signal_off(JWidget widget)
{
  assert_valid_widget(widget);

  widget->emit_signals++;
}

int jwidget_emit_signal(JWidget widget, int signal_num)
{
  assert_valid_widget(widget);

  if (!widget->emit_signals) {
    JMessage msg;
    int ret;

#ifdef REPORT_SIGNALS
    printf("Signal: %d (%d)\n", signal_num, widget->id);
#endif

    msg = jmessage_new(JM_SIGNAL);
    msg->signal.num = signal_num;
    msg->signal.from = widget;

    ret = jwidget_send_message(widget, msg);

    /* send the signal to the window too */
    if (!ret && widget->type != JI_WINDOW) {
      JWidget window = jwidget_get_window(widget);
      if (window)
	ret = jwidget_send_message(window, msg);
    }

    jmessage_free(msg);
    return ret;
  }
  else
    return 0;
}

/**********************************************************************/
/* manager handler */

#define SENDMSG()					\
  if (hook->msg_proc) {					\
    done = (*hook->msg_proc)(widget, msg);		\
    if (done)						\
      break;						\
  }

bool jwidget_send_message(JWidget widget, JMessage msg)
{
  bool done = FALSE;
  JHook hook;
  JLink link;

  assert_valid_widget(widget);
  assert(msg != NULL);

  JI_LIST_FOR_EACH(widget->hooks, link) {
    hook = reinterpret_cast<JHook>(link->data);
    SENDMSG();
  }

  return done;
}

bool jwidget_send_message_after_type(JWidget widget, JMessage msg, int type)
{
  bool done = FALSE;
  bool send = FALSE;
  JHook hook;
  JLink link;

  assert_valid_widget(widget);
  assert(msg != NULL);

  JI_LIST_FOR_EACH(widget->hooks, link) {
    hook = reinterpret_cast<JHook>(link->data);

    if (hook->type == type) {
      send = TRUE;
      continue;
    }
    else if (!send)
      continue;

    SENDMSG();
  }

  return done;
}

void jwidget_close_window(JWidget widget)
{
  JWidget window;

  assert_valid_widget(widget);

  window = jwidget_get_window(widget);  
  if (window)
    jwindow_close(window, widget);
}

/**
 * Captures the mouse to send the future JM_BUTTONRELEASED messsage to
 * the specified widget. There are messages like JM_MOTION and
 * JM_SETCURSOR that are sent normally to the widget with the mouse
 * (and not with the "soft" capture).
 *
 * @see jwidget_hard_capture_mouse
 */
void jwidget_capture_mouse(JWidget widget)
{
  assert_valid_widget(widget);

  if (!jmanager_get_capture()) {
    jmanager_set_capture(widget);

    if (jmanager_get_capture() == widget)
      widget->flags &= ~JI_HARDCAPTURE;
  }
}

/**
 * Captures the mouse to send all the future mouse messsages to the
 * specified widget (included the JM_MOTION and JM_SETCURSOR).
 * 
 * @see jwidget_capture_mouse
 */
void jwidget_hard_capture_mouse(JWidget widget)
{
  assert_valid_widget(widget);

  if (!jmanager_get_capture()) {
    jmanager_set_capture(widget);
#ifdef ALLEGRO_WINDOWS
    SetCapture(win_get_window());
#endif

    if (jmanager_get_capture() == widget)
      widget->flags |= JI_HARDCAPTURE;
  }
}

/**
 * Releases the capture of the mouse events.
 */
void jwidget_release_mouse(JWidget widget)
{
  assert_valid_widget(widget);

  if (jmanager_get_capture() == widget) {
    jmanager_free_capture();
#ifdef ALLEGRO_WINDOWS
    ReleaseCapture();
#endif

    widget->flags &= ~JI_HARDCAPTURE;
  }
}

/**********************************************************************/
/* miscellaneous */

JWidget jwidget_find_name(JWidget widget, const char *name)
{
  JWidget child;
  JLink link;

  assert_valid_widget(widget);

  JI_LIST_FOR_EACH(widget->children, link) {
    child = (JWidget)link->data;
    if (child->name != NULL && strcmp(child->name, name) == 0)
      return child;
  }

  JI_LIST_FOR_EACH(widget->children, link) {
    if ((child = jwidget_find_name((JWidget)link->data, name)))
      return child;
  }

  return 0;
}

bool jwidget_check_underscored(JWidget widget, int scancode)
{
  int c, ascii;

  assert_valid_widget(widget);

  ascii = 0;
  if (scancode >= KEY_0 && scancode <= KEY_9)
    ascii = '0' + (scancode - KEY_0);
  else if (scancode >= KEY_A && scancode <= KEY_Z)
    ascii = 'a' + (scancode - KEY_A);
  else
    return FALSE;

  if (widget->text) {
    for (c=0; widget->text[c]; c++)
      if ((widget->text[c] == '&') && (widget->text[c+1] != '&'))
	if (ascii == tolower(widget->text[c+1]))
	  return TRUE;
  }

  return FALSE;
}

/**********************************************************************/
/* widget message procedure */

static bool widget_msg_proc(JWidget widget, JMessage msg)
{
  assert(msg != NULL);
  assert_valid_widget(widget);

  switch (msg->type) {

    case JM_OPEN:
    case JM_CLOSE:
    case JM_WINMOVE: {
      JLink link;

      /* broadcast the message to the children */
      JI_LIST_FOR_EACH(widget->children, link)
	jwidget_send_message(reinterpret_cast<JWidget>(link->data), msg);
      break;
    }

    case JM_DRAW:
      if (widget->draw_method) {
	(*widget->draw_method)(widget, &msg->draw.rect);
	return TRUE;
      }
      break;

    case JM_REQSIZE:
      msg->reqsize.w = widget->min_w;
      msg->reqsize.h = widget->min_h;
      return TRUE;

    case JM_SETPOS: {
      JRect cpos;
      JLink link;

      jrect_copy(widget->rc, &msg->setpos.rect);
      cpos = jwidget_get_child_rect(widget);

      /* set all the children to the same "cpos" */
      JI_LIST_FOR_EACH(widget->children, link)
	jwidget_set_rect(reinterpret_cast<JWidget>(link->data), cpos);

      jrect_free(cpos);
      return TRUE;
    }

    case JM_DRAWRGN:
#if 0
      {
	int redraw = FALSE;
	JRegion region2;
	JMessage msg2;
	JRect rect;
	JLink link;
	int count;

	for (it=msg->drawrgn.region->rects; it; it=it->next) {
	  rect = it->data;
	  if ((widget->rc->x <= rect->x+rect->w-1) &&
	      (widget->rc->y <= rect->y+rect->h-1) &&
	      (widget->rc->x+widget->rc->w-1 >= rect->x) &&
	      (widget->rc->y+widget->rc->h-1 >= rect->y)) {
	    redraw = TRUE;
	    break;
	  }
	}

	if (redraw) {
	  /* get areas to draw */
	  region2 = jwidget_get_drawable_region(widget, 0);
	  jregion_intersect2 (region2, msg->drawrgn.region);

	  /* draw the widget */
	  count = jlist_length(region2->rects);
	  for (it=region2->rects; it; it=it->next) {
	    /* create the draw message */
	    msg2 = jmessage_new(JM_DRAW);
	    msg2->draw.count = --count;
	    jrect_copy(&msg2->draw.rect, (JRect)it->data);
	    jmessage_add_dest(msg2, widget);

	    /* enqueue message */
	    jmanager_enqueue_message(msg2);
	  }

	  jregion_free(region2);

	  /* send message to children */
	  JI_LIST_FOR_EACH(widget->children, link)
	    jwidget_send_message((JWidget)link->data, msg);
	}
      }
#else
      if (!(widget->flags & JI_HIDDEN)) /* is visible? */
	jwidget_invalidate_region(widget, msg->drawrgn.region);
#endif
      return TRUE;

    case JM_DIRTYCHILDREN: {
      JLink link;

      JI_LIST_FOR_EACH(widget->children, link)
	jwidget_dirty(reinterpret_cast<JWidget>(link->data));

      return TRUE;
    }

    case JM_KEYPRESSED:
    case JM_KEYRELEASED:
      if (msg->key.propagate_to_children) {
	JLink link;

	/* broadcast the message to the children */
	JI_LIST_FOR_EACH(widget->children, link)
	  jwidget_send_message(reinterpret_cast<JWidget>(link->data), msg);
      }

      /* propagate the message to the parent */
      if (msg->key.propagate_to_parent && widget->parent != NULL)
	return jwidget_send_message(widget->parent, msg);
      else
	break;

    case JM_BUTTONPRESSED:
    case JM_BUTTONRELEASED:
    case JM_DOUBLECLICK:
    case JM_MOTION:
    case JM_WHEEL:
      /* propagate the message to the parent */
      if (widget->parent != NULL)
	return jwidget_send_message(widget->parent, msg);
      else
	break;

    case JM_SETCURSOR:
      /* propagate the message to the parent */
      if (widget->parent != NULL)
	return jwidget_send_message(widget->parent, msg);
      else {
	jmouse_set_cursor(JI_CURSOR_NORMAL);
	return TRUE;
      }

  }

  return FALSE;
}