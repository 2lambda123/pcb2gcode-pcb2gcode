/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
 *
 * pcb2gcode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * pcb2gcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EXPORTER_H
#define EXPORTER_H

#include <memory>                      // for shared_ptr
#include "boost/core/noncopyable.hpp"  // for noncopyable
class Board;
namespace boost { namespace program_options { class variables_map; } }

class Exporter: public boost::noncopyable {
 public:
  Exporter(std::shared_ptr<Board> board) {}
  virtual void export_all(boost::program_options::variables_map&) = 0;
  virtual ~Exporter() {}
};

#endif // EXPORTER_H
