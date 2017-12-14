/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2016 Nicola Corna <nicola@corna.info>
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

#include <fstream>
#include <limits>
using std::numeric_limits;

#include <iostream>
using std::cerr;
using std::endl;

#include <boost/format.hpp>

#include <glibmm/miscutils.h>
using Glib::build_filename;

#include "tsp_solver.hpp"
#include "surface_vectorial.hpp"
#include "eulerian_paths.hpp"
//#include "buffer_end_none.hpp"
using std::max;
using std::max_element;
using std::next;

unsigned int Surface_vectorial::debug_image_index = 0;

Surface_vectorial::Surface_vectorial(unsigned int points_per_circle, ivalue_t width,
                                        ivalue_t height, string name, string outputdir) :
    points_per_circle(points_per_circle),
    width_in(width),
    height_in(height),
    name(name),
    outputdir(outputdir),
    fill(false)
{

}

void Surface_vectorial::render(shared_ptr<VectorialLayerImporter> importer)
{
    unique_ptr<multi_polygon_type> vectorial_surface_not_simplified;

    vectorial_surface = make_shared<multi_polygon_type>();
    vectorial_surface_not_simplified = importer->render(fill, points_per_circle);

    if (bg::intersects(*vectorial_surface_not_simplified))
        throw std::logic_error("Input geometry is self-intersecting");

    scale = importer->vectorial_scale();

    //With a very small loss of precision we can reduce memory usage and processing time
    bg::simplify(*vectorial_surface_not_simplified, *vectorial_surface, scale / 10000);
    bg::envelope(*vectorial_surface, bounding_box);
}

vector<shared_ptr<icoords> > Surface_vectorial::get_toolpath(shared_ptr<RoutingMill> mill,
        bool mirror)
{
    coordinate_type tolerance = mill->tolerance * scale;
    // This is by how much we will grow each trace if extra passes are needed.
    coordinate_type grow = mill->tool_diameter / 2 * scale;

    shared_ptr<Isolator> isolator = dynamic_pointer_cast<Isolator>(mill);
    // extra passes are done on each trace if requested, each offset by half the tool diameter.
    const int extra_passes = isolator ? isolator->extra_passes : 0;

    if (tolerance <= 0)
        tolerance = 0.0001 * scale;

    bg::unique(*vectorial_surface);
    multi_linestring_type_fp voronoi_edges =
           Voronoi::get_voronoi_edges(*vectorial_surface, bounding_box, tolerance);

    struct PointLessThan {
      bool operator()(const point_type_fp& a, const point_type_fp& b) const {
        return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
      }
    };
    voronoi_edges = get_eulerian_paths<point_type_fp, linestring_type_fp, multi_linestring_type_fp, PointLessThan>(voronoi_edges);
    box_type svg_bounding_box;

    // Make the svg file large enough to contains the width of all milling.
    if (grow > 0)
        bg::buffer(bounding_box, svg_bounding_box, grow * (extra_passes + 1));
    else
        bg::assign(svg_bounding_box, bounding_box);

    const string traced_filename = (boost::format("outp%d_traced_%s.svg") % debug_image_index++ % name).str();
    svg_writer debug_image(build_filename(outputdir, "processed_" + name + ".svg"), SVG_PIX_PER_IN, scale, svg_bounding_box);
    svg_writer traced_debug_image(build_filename(outputdir, traced_filename), SVG_PIX_PER_IN, scale, svg_bounding_box);

    srand(1);
    traced_debug_image.add(voronoi_edges, 0.3, true);

    const coordinate_type mirror_axis = mill->mirror_absolute ?
        bounding_box.min_corner().x() :
        ((bounding_box.min_corner().x() + bounding_box.max_corner().x()) / 2);
    bool contentions = false;

    srand(1);

    vector<shared_ptr<icoords> > toolpath;
    vector<shared_ptr<icoords> > toolpath_optimised;

    auto copy_mls_to_toolpath = [&](const multi_linestring_type_fp& mls) {
        for (const auto& ls : mls) {
            toolpath.push_back(make_shared<icoords>());
            for (const auto& point : ls) {
                if (mirror) {
                    toolpath.back()->push_back(make_pair((2 * mirror_axis - point.x()) / double(scale),
                                                         point.y() / double(scale)));
                } else {
                    toolpath.back()->push_back(make_pair(point.x() / double(scale),
                                                         point.y() / double(scale)));
                }
            }
        }
    };
    // First get all the segments for a mask.
    multi_segment_type mask_segments;
    if (mask) {
        auto mask_polys = mask->vectorial_surface;
        for (const auto& mask_poly : *mask_polys) {
            for (size_t i = 1; i < mask_poly.outer().size(); i++) {
                mask_segments.push_back(segment_type(mask_poly.outer()[i-1], mask_poly.outer()[i]));
            }
            for (const auto& inner_ring : mask_poly.inners()) {
                for (size_t i = 1; i < inner_ring.size(); i++) {
                    mask_segments.push_back(segment_type(inner_ring[i-1], inner_ring[i]));
                }
            }
        }
    } else {
        // if there's no mask, we'll use the convex hull as a mask.
        ring_type_fp convex_hull;
        bg::convex_hull(voronoi_edges, convex_hull);
    }
    multi_segment_type clipped_voronoi_edges;
        
        //bg::intersection(clipped_voronoi_edges);
        //std::cout << bg::wkt(clipped_voronoi_edges) << std::endl;
        //bg::intersect_segments(a.cbegin(), a.cend(), b);
        //voronoi_edges = clipped_voronoi_edges;

    copy_mls_to_toolpath(voronoi_edges);
    if (grow > 0) {
        for (int i = 0; i < extra_passes; i++) {
            for (const linestring_type_fp& voronoi_edge : voronoi_edges) {
                // For each edge, we need to make successive edges that are
                // offset by grow on each side.  The number of edges that need
                // to be made depends on the number of extra_passes.
                //const unsigned int r = rand() % 256;
                //const unsigned int g = rand() % 256;
                //const unsigned int b = rand() % 256;
                // This is how far off the current path that we want to offset
                coordinate_type current_grow = grow * (i + 1);
                multi_polygon_type_fp buffered_linestring;
                bg::buffer(voronoi_edge, buffered_linestring,
                           bg::strategy::buffer::distance_symmetric<coordinate_type>(current_grow),
                           bg::strategy::buffer::side_straight(),
                           bg::strategy::buffer::join_round(points_per_circle),
                           //bg::strategy::buffer::join_miter(numeric_limits<coordinate_type>::max()),
                           bg::strategy::buffer::end_flat(),
                           bg::strategy::buffer::point_circle(points_per_circle));
                // The buffered_linestring is now an oval surrounding the original path.  Let's extract all paths from it.
                multi_linestring_type_fp mls;
                for (const polygon_type_fp& poly : buffered_linestring) {
                    mls.push_back(linestring_type_fp(poly.outer().cbegin(), poly.outer().cend()));
                    for (const ring_type_fp& inner : poly.inners()) {
                        mls.push_back(linestring_type_fp(inner.cbegin(), inner.cend()));
                    }
                }
                traced_debug_image.add(mls, 0.3, true);
                copy_mls_to_toolpath(mls);
            }
        }
    }

    srand(1);
    debug_image.add(*vectorial_surface, 1, true);

    if (contentions)
    {
        cerr << "\nWarning: pcb2gcode hasn't been able to fulfill all"
             << " clearance requirements and tried a best effort approach"
             << " instead. You may want to check the g-code output and"
             << " possibly use a smaller milling width.\n";
    }

    tsp_solver::nearest_neighbour( toolpath, std::make_pair(0, 0), 0.0001 );

    if (mill->optimise)
    {
        for (const shared_ptr<icoords>& ring : toolpath)
        {
            toolpath_optimised.push_back(make_shared<icoords>());
            bg::simplify(*ring, *(toolpath_optimised.back()), mill->tolerance);
        }

        return toolpath_optimised;
    }
    else
        return toolpath;
}

void Surface_vectorial::save_debug_image(string message)
{
    const string filename = (boost::format("outp%d_%s.svg") % debug_image_index % message).str();
    svg_writer debug_image(build_filename(outputdir, filename), SVG_PIX_PER_IN, scale, bounding_box);

    srand(1);
    debug_image.add(*vectorial_surface, 1, true);

    ++debug_image_index;
}

void Surface_vectorial::enable_filling()
{
    fill = true;
}

void Surface_vectorial::add_mask(shared_ptr<Core> surface)
{
    mask = dynamic_pointer_cast<Surface_vectorial>(surface);

    if (mask)
    {
        auto masked_surface = make_shared<multi_polygon_type>();

        bg::intersection(*vectorial_surface, *(mask->vectorial_surface), *masked_surface);
        vectorial_surface = masked_surface;

        bg::envelope(*(mask->vectorial_surface), bounding_box);
    }
    else
        throw std::logic_error("Can't cast Core to Surface_vectorial");
}

svg_writer::svg_writer(string filename, unsigned int pixel_per_in, coordinate_type scale, box_type bounding_box) :
    output_file(filename),
    bounding_box(bounding_box)
{
    const coordinate_type width =
        (bounding_box.max_corner().x() - bounding_box.min_corner().x()) * pixel_per_in / scale;
    const coordinate_type height =
        (bounding_box.max_corner().y() - bounding_box.min_corner().y()) * pixel_per_in / scale;

    //Some SVG readers does not behave well when viewBox is not specified
    const string svg_dimensions =
        str(boost::format("width=\"%1%\" height=\"%2%\" viewBox=\"0 0 %1% %2%\"") % width % height);

    mapper = unique_ptr<bg::svg_mapper<point_type> >
        (new bg::svg_mapper<point_type>(output_file, width, height, svg_dimensions));
    mapper->add(bounding_box);
}

void svg_writer::add(const multi_polygon_type& geometry, double opacity, bool stroke)
{
    string stroke_str = stroke ? "stroke:rgb(0,0,0);stroke-width:2" : "";

    for (const polygon_type& poly : geometry)
    {
        const unsigned int r = rand() % 256;
        const unsigned int g = rand() % 256;
        const unsigned int b = rand() % 256;
        multi_polygon_type mpoly;

        bg::intersection(poly, bounding_box, mpoly);

        mapper->map(mpoly,
            str(boost::format("fill-opacity:%f;fill:rgb(%u,%u,%u);" + stroke_str) %
            opacity % r % g % b));
    }
}

void svg_writer::add(const multi_linestring_type_fp& geometry, double opacity, bool stroke)
{
    string stroke_str = stroke ? "stroke:rgb(0,0,0);stroke-width:2" : "";

    for (const linestring_type_fp& linestring : geometry)
    {
        multi_linestring_type_fp mlinestring;
        bg::intersection(linestring, bounding_box, mlinestring);

        const unsigned int r = rand() % 256;
        const unsigned int g = rand() % 256;
        const unsigned int b = rand() % 256;
        mapper->map(mlinestring,
            str(boost::format("fill-opacity:%f;fill:rgb(%u,%u,%u);" + stroke_str) %
            opacity % r % g % b));
    }
}

void svg_writer::add(const vector<polygon_type>& geometries, double opacity, int r, int g, int b)
{
    if (r < 0 || g < 0 || b < 0)
    {
        r = rand() % 256;
        g = rand() % 256;
        b = rand() % 256;
    }

    for (unsigned int i = geometries.size(); i != 0; i--)
    {
        multi_polygon_type mpoly;

        bg::intersection(geometries[i - 1], bounding_box, mpoly);

        if (i == geometries.size())
        {
            mapper->map(mpoly,
                str(boost::format("fill-opacity:%f;fill:rgb(%u,%u,%u);stroke:rgb(0,0,0);stroke-width:2") %
                opacity % r % g % b));
        }
        else
        {
            mapper->map(mpoly, "fill:none;stroke:rgb(0,0,0);stroke-width:1");
        }
    }
}

