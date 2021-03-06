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

#ifndef RASTER_PEN_H_INCLUDED
#define RASTER_PEN_H_INCLUDED
#pragma once

#include "gfx/point.h"
#include "gfx/rect.h"
#include "raster/pen_type.h"
#include <vector>

namespace raster {

  class Image;

  struct PenScanline {
    int state, x1, x2;
  };

  class Pen {
  public:
    Pen();
    Pen(PenType type, int size, int angle);
    Pen(const Pen& pen);
    ~Pen();

    PenType get_type() const { return m_type; }
    int get_size() const { return m_size; }
    int get_angle() const { return m_angle; }
    Image* get_image() { return m_image; }
    const std::vector<PenScanline>& get_scanline() const { return m_scanline; }

    const gfx::Rect& getBounds() const { return m_bounds; }

    void set_type(PenType type);
    void set_size(int size);
    void set_angle(int angle);

  private:
    void clean_pen();
    void regenerate_pen();

    PenType m_type;                       // Type of pen
    int m_size;                           // Size (diameter)
    int m_angle;                          // Angle in degrees 0-360
    Image* m_image;                       // Image of the pen
    std::vector<PenScanline> m_scanline;
    gfx::Rect m_bounds;
  };

} // namespace raster

#endif
