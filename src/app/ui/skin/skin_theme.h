/* Aseprite
 * Copyright (C) 2001-2013  David Capello
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

#ifndef APP_UI_SKIN_SKIN_THEME_H_INCLUDED
#define APP_UI_SKIN_SKIN_THEME_H_INCLUDED
#pragma once

#include "app/ui/skin/skin_part.h"
#include "app/ui/skin/skin_parts.h"
#include "app/ui/skin/style_sheet.h"
#include "base/compiler_specific.h"
#include "gfx/fwd.h"
#include "ui/color.h"
#include "ui/manager.h"
#include "ui/system.h"
#include "ui/theme.h"

#include <map>
#include <string>

#include <allegro/color.h>

namespace ui {
  class Entry;
  class Graphics;
  class IButtonIcon;
}

namespace app {
  namespace skin {

    namespace ThemeColor {
      enum Type {
        Text,
        Disabled,
        Face,
        HotFace,
        Selected,
        Background,
        TextBoxText,
        TextBoxFace,
        EntrySuffix,
        LinkText,
        ButtonNormalText,
        ButtonNormalFace,
        ButtonHotText,
        ButtonHotFace,
        ButtonSelectedText,
        ButtonSelectedFace,
        CheckHotFace,
        CheckFocusFace,
        RadioHotFace,
        RadioFocusFace,
        MenuItemNormalText,
        MenuItemNormalFace,
        MenuItemHotText,
        MenuItemHotFace,
        MenuItemHighlightText,
        MenuItemHighlightFace,
        EditorFace,
        EditorSpriteBorder,
        EditorSpriteBottomBorder,
        ListItemNormalText,
        ListItemNormalFace,
        ListItemSelectedText,
        ListItemSelectedFace,
        SliderEmptyText,
        SliderEmptyFace,
        SliderFullText,
        SliderFullFace,
        TabNormalText,
        TabNormalFace,
        TabSelectedText,
        TabSelectedFace,
        SplitterNormalFace,
        ScrollBarBgFace,
        ScrollBarThumbFace,
        PopupWindowBorder,
        TooltipText,
        TooltipFace,
        FileListEvenRowText,
        FileListEvenRowFace,
        FileListOddRowText,
        FileListOddRowFace,
        FileListSelectedRowText,
        FileListSelectedRowFace,
        FileListDisabledRowText,
        Workspace,
        MaxColors
      };
    } 

    extern const char* kWindowFaceColorId;

    // This is the GUI theme used by Aseprite (which use images from
    // data/skins directory).
    class SkinTheme : public ui::Theme {
    public:
      static const char* kThemeCloseButtonId;

      SkinTheme();
      ~SkinTheme();

      ui::Color getColor(ThemeColor::Type k) const {
        return m_colors[k];
      }

      FONT* getMiniFont() const { return m_minifont; }

      void reload_skin();
      void reload_fonts();

      ui::Cursor* getCursor(ui::CursorType type);
      void initWidget(ui::Widget* widget);
      void getWindowMask(ui::Widget* widget, gfx::Region& region);
      void setDecorativeWidgetBounds(ui::Widget* widget);

      void paintDesktop(ui::PaintEvent& ev);
      void paintBox(ui::PaintEvent& ev);
      void paintButton(ui::PaintEvent& ev);
      void paintCheckBox(ui::PaintEvent& ev);
      void paintEntry(ui::PaintEvent& ev);
      void paintGrid(ui::PaintEvent& ev);
      void paintLabel(ui::PaintEvent& ev);
      void paintLinkLabel(ui::PaintEvent& ev);
      void paintListBox(ui::PaintEvent& ev);
      void paintListItem(ui::PaintEvent& ev);
      void paintMenu(ui::PaintEvent& ev);
      void paintMenuItem(ui::PaintEvent& ev);
      void paintSplitter(ui::PaintEvent& ev);
      void paintRadioButton(ui::PaintEvent& ev);
      void paintSeparator(ui::PaintEvent& ev);
      void paintSlider(ui::PaintEvent& ev);
      void paintComboBoxEntry(ui::PaintEvent& ev);
      void paintComboBoxButton(ui::PaintEvent& ev);
      void paintTextBox(ui::PaintEvent& ev);
      void paintView(ui::PaintEvent& ev);
      void paintViewScrollbar(ui::PaintEvent& ev);
      void paintViewViewport(ui::PaintEvent& ev);
      void paintWindow(ui::PaintEvent& ev);
      void paintPopupWindow(ui::PaintEvent& ev);
      void paintWindowButton(ui::PaintEvent& ev);
      void paintTooltip(ui::PaintEvent& ev);

      int get_button_selected_offset() const { return 0; } // TODO Configurable in xml

      BITMAP* get_part(int part_i) const { return m_part[part_i]; }
      BITMAP* get_toolicon(const char* tool_id) const;
      gfx::Size get_part_size(int part_i) const;

      // Helper functions to draw bounds/hlines with sheet parts
      void draw_bounds_array(ui::Graphics* g, const gfx::Rect& rc, int parts[8]);
      void draw_bounds_nw(ui::Graphics* g, const gfx::Rect& rc, int nw, ui::Color bg = ui::ColorNone);
      void draw_bounds_nw(ui::Graphics* g, const gfx::Rect& rc, const SkinPartPtr skinPart, ui::Color bg = ui::ColorNone);
      void draw_bounds_nw2(ui::Graphics* g, const gfx::Rect& rc, int x_mid, int nw1, int nw2, ui::Color bg1, ui::Color bg2);
      void draw_part_as_hline(ui::Graphics* g, const gfx::Rect& rc, int part);
      void draw_part_as_vline(ui::Graphics* g, const gfx::Rect& rc, int part);
      void paintProgressBar(ui::Graphics* g, const gfx::Rect& rc, float progress);

      Style* getStyle(const std::string& id) {
        return m_stylesheet.getStyle(id);
      }

      SkinPartPtr getPartById(const std::string& id) {
        return m_parts_by_id[id];
      }

      ui::Color getColorById(const std::string& id) {
        return m_colors_by_id[id];
      }

    protected:
      void onRegenerate() OVERRIDE;

    private:
      void draw_bounds_template(ui::Graphics* g, const gfx::Rect& rc,
                                int nw, int n, int ne, int e, int se, int s, int sw, int w);
      void draw_bounds_template(ui::Graphics* g, const gfx::Rect& rc, const SkinPartPtr& skinPart);
      void draw_bounds_template(ui::Graphics* g, const gfx::Rect& rc,
                                BITMAP* nw, BITMAP* n, BITMAP* ne,
                                BITMAP* e, BITMAP* se, BITMAP* s,
                                BITMAP* sw, BITMAP* w);

      BITMAP* cropPartFromSheet(BITMAP* bmp, int x, int y, int w, int h);
      ui::Color getWidgetBgColor(ui::Widget* widget);
      void drawTextString(ui::Graphics* g, const char *t, ui::Color fg_color, ui::Color bg_color,
                          bool fill_bg, ui::Widget* widget, const gfx::Rect& rc,
                          int selected_offset);
      void drawEntryCaret(ui::Graphics* g, ui::Entry* widget, int x, int y);

      void paintIcon(ui::Widget* widget, ui::Graphics* g, ui::IButtonIcon* iconInterface, int x, int y);

      static FONT* loadFont(const char* userFont, const std::string& path);

      std::string m_selected_skin;
      BITMAP* m_sheet_bmp;
      std::vector<BITMAP*> m_part;
      std::map<std::string, SkinPartPtr> m_parts_by_id;
      std::map<std::string, BITMAP*> m_toolicon;
      std::map<std::string, ui::Color> m_colors_by_id;
      std::vector<ui::Cursor*> m_cursors;
      std::vector<ui::Color> m_colors;
      StyleSheet m_stylesheet;
      FONT* m_minifont;
    };

    inline Style* get_style(const std::string& id) {
      return static_cast<SkinTheme*>(ui::Manager::getDefault()->getTheme())->getStyle(id);
    }

    inline SkinPartPtr get_part_by_id(const std::string& id) {
      return static_cast<SkinTheme*>(ui::Manager::getDefault()->getTheme())->getPartById(id);
    }

    inline ui::Color get_color_by_id(const std::string& id) {
      return static_cast<SkinTheme*>(ui::Manager::getDefault()->getTheme())->getColorById(id);
    }

  } // namespace skin
} // namespace app

#endif
