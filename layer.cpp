/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
 * Copyright (C) 2015 Nicola Corna <nicola@corna.info>
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

#include "layer.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
Layer::Layer(const string& name, shared_ptr<Core> surface,
             shared_ptr<RoutingMill> manufacturer, bool backside)
{
    this->name = name;
    this->mirrored = backside;
    this->surface = surface;
    this->manufacturer = manufacturer;
}

#include <iostream>

/******************************************************************************/
/*
 */
/******************************************************************************/
vector<shared_ptr<icoords> > Layer::get_toolpaths()
{
    return surface->get_toolpath(manufacturer, mirrored);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
shared_ptr<RoutingMill> Layer::get_manufacturer()
{
    return manufacturer;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void Layer::add_mask(shared_ptr<Layer> mask)
{
    surface->add_mask(mask->surface);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
vector<unsigned int> Layer::get_bridges( shared_ptr<icoords> toolpath )
{
    return surface->get_bridges(dynamic_pointer_cast<Cutter>( manufacturer ), toolpath);
}

