/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "module.h"
#include "ModuleInstantiation.h"
#include "node.h"
#include "polyset.h"
#include "evalcontext.h"
#include "builtin.h"
#include "printutils.h"
#include "fileutils.h"
#include "handle_dep.h" // handle_dep()
#include "lodepng.h"
#include "FactoryNode.h"

#include <sstream>
#include <fstream>
#include <unordered_map>
#include <boost/functional/hash.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/assign/std/vector.hpp>
using namespace boost::assign; // bring 'operator+=()' into scope

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

typedef std::unordered_map<std::pair<int,int>, double, boost::hash<std::pair<int,int>>> img_data_t;

class SurfaceNode : public FactoryNode
{
public:
	SurfaceNode() : FactoryNode("file", "center", "convexity", "r", "nonzero")
		, center(false), invert(false), nonZero(false), vertCorners(false)
		, convexity(0), height(1)
	{ }

	Filename filename;
	bool center;
	bool invert;
	bool nonZero;
	bool vertCorners;
	int convexity;
	double height;
	Vector2d r0, r1;

private:
	mutable img_data_t data;
	mutable int lines;
	mutable int columns;
	mutable double min_val;

	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		ValuePtr fileval = c.lookup_variable("file");
		this->filename = lookup_file(fileval->isUndefined() ? "" : fileval->toString(), evalctx.location().path(), c.documentPath());

		ValuePtr center = c.lookup_variable("center", true);
		if (center->type() == Value::BOOL) {
			this->center = center->toBool();
		}

		ValuePtr convexity = c.lookup_variable("convexity", true);
		if (convexity->type() == Value::NUMBER) {
			this->convexity = (int)convexity->toDouble();
		}

		ValuePtr invert = c.lookup_variable("invert", true);
		if (invert->type() == Value::BOOL) {
			this->invert = invert->toBool();
		}

		this->vertCorners = true;
		ValuePtr vertCorners = c.lookup_variable("vertCorners", true);
		if (vertCorners->type() == Value::BOOL) {
			this->vertCorners = vertCorners->toBool();
		}

		this->height = 100;
		ValuePtr height = c.lookup_variable("height", true);
		if (height->type() == Value::NUMBER) {
			this->height = height->toDouble();
		}

		this->r0 = Vector2d(0, 0);
		this->r1 = Vector2d(0, 0);

		ValuePtr r = c.lookup_variable("r", true);
		if (r->type() == Value::NUMBER) {
			r->getFiniteDouble(this->r0[0]);
			r->getFiniteDouble(this->r0[1]);
		}
		else if (r->type() == Value::VECTOR) {
			r->getVec2(this->r0[0], this->r0[1], true);
		}
		else {
			ValuePtr r0 = c.lookup_variable("r0", true);
			r0->getFiniteDouble(this->r0[0]);
			r0->getFiniteDouble(this->r0[1]);
			r0->getVec2(this->r0[0], this->r0[1], true);

			ValuePtr r1 = c.lookup_variable("r1", true);
			r1->getFiniteDouble(this->r1[0]);
			r1->getFiniteDouble(this->r1[1]);
			r1->getVec2(this->r1[0], this->r1[1], true);
		}

		this->nonZero = false;
		ValuePtr nonzero = c.lookup_variable("nonzero", true);
		if (nonzero->type() == Value::BOOL) {
			this->nonZero = nonzero->toBool();
		}

		// add timestamp to the context
		fs::path path((std::string)this->filename);
		time_t ts = fs::exists(path) ? fs::last_write_time(path) : 0;
		c.set_variable("timestamp", ValuePtr((double)ts));
	}

	void convert_image(img_data_t &data, std::vector<unsigned char> &img, unsigned int width, unsigned int height) const
	{
		for (unsigned int y = 0; y < height; y++) {
			for (unsigned int x = 0; x < width; x++) {
				long idx = 4 * (y * width + x);
				double pixel = 0.2126 * img[idx] + 0.7152 * img[idx + 1] + 0.0722 * img[idx + 2];
				double z = this->height / 255 * (invert ? 255 - pixel : pixel);
				data[std::make_pair(height - 1 - y, x)] = z;
			}
		}
	}

	bool is_png(std::vector<unsigned char> &png) const
	{
		return (png.size() >= 8)
			&& (png[0] == 0x89)
			&& (png[1] == 0x50)
			&& (png[2] == 0x4e)
			&& (png[3] == 0x47)
			&& (png[4] == 0x0d)
			&& (png[5] == 0x0a)
			&& (png[6] == 0x1a)
			&& (png[7] == 0x0a);
	}

	img_data_t read_png_or_dat(std::string filename) const
	{
		img_data_t data;
		std::vector<unsigned char> png;

		lodepng::load_file(png, filename);

		if (!is_png(png)) {
			png.clear();
			return read_dat(filename);
		}

		unsigned int width, height;
		std::vector<unsigned char> img;
		unsigned error = lodepng::decode(img, width, height, png);
		if (error) {
			PRINTB("ERROR: Can't read PNG image '%s'", filename);
			data.clear();
			return data;
		}

		convert_image(data, img, width, height);

		return data;
	}

	img_data_t read_dat(std::string filename) const
	{
		img_data_t data;
		std::ifstream stream(filename.c_str());

		if (!stream.good()) {
			PRINTB("WARNING: Can't open DAT file '%s'.", filename);
			return data;
		}

		int lines = 0, columns = 0;
		double min_val = 0;

		typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
		boost::char_separator<char> sep(" \t");

		while (!stream.eof()) {
			std::string line;
			while (!stream.eof() && (line.size() == 0 || line[0] == '#')) {
				std::getline(stream, line);
				boost::trim(line);
			}
			if (line.size() == 0 && stream.eof()) break;

			int col = 0;
			tokenizer tokens(line, sep);
			try {
				for (const auto &token : tokens) {
					double v = boost::lexical_cast<double>(token);
					data[std::make_pair(lines, col++)] = v;
					if (col > columns) columns = col;
					min_val = std::min(v - 1, min_val);
				}
			}
			catch (const boost::bad_lexical_cast &blc) {
				if (!stream.eof()) {
					PRINTB("WARNING: Illegal value in '%s': %s", filename % blc.what());
				}
				break;
			}
			lines++;
		}

		return data;
	}

	double getRadius(int x, int y, double t) const
	{
		if (r0[0] == 0 && r0[1] == 0)
			return t;

		const auto &radius = r0 * t + r1 * (1 - t);

		double xx = x * 2.0 / (columns - 1) - 1.0;
		double yy = y * 2.0 / (lines - 1) - 1.0;

		double rx = radius[0] * (columns / 2) * std::sqrt(1 - xx * xx);
		double ry = radius[1] * (lines / 2) * std::sqrt(1 - yy * yy);

		double result = std::sqrt(rx * rx + ry * ry);

		return result;
	}

	double getHeight(int x, int y) const
	{
		double t = data[std::make_pair(y, x)] / height;
		double result = getRadius(x, y, t) * height;
		return result;
	}

	Vector3d getVec(int x, int y, double t) const
	{
		double z = getRadius(x, y, t) * height;

		double cx = (columns - 1) / 2.0;
		double cy = (lines - 1) / 2.0;

		double ox = center ? 0 : cx;
		double oy = center ? 0 : cy;

		const auto &radius = r0[0] == 0 && r0[1] == 0 ? Vector2d(1, 1) : r0 * t + r1 * (1 - t);
		return Vector3d((x - cx) * radius[0] + ox, (y - cy) * radius[1] + oy, z);
	}

	Vector3d getVec(int x, int y) const
	{
		double z = getHeight(x, y);

		double cx = (columns - 1) / 2.0;
		double cy = (lines - 1) / 2.0;

		double ox = center ? 0 : cx;
		double oy = center ? 0 : cy;

		double t = data[std::make_pair(y, x)] / height;
		const auto &radius = r0[0] == 0 && r0[1] == 0 ? Vector2d(1, 1) : r0 * t + r1 * (1 - t);
		return Vector3d((x - cx) * radius[0] + ox, (y - cy) * radius[1] + oy, z);
	}

	double getVecs(int x, int y, Vector3d &top, Vector3d &bot) const
	{
		top = getVec(x, y);
		bot = getVec(x, y, 0);
		double t = data[std::make_pair(y, x)] / height;
		return t;
	}

	inline bool almost(double a, double b, double tol = 1.0 / 512.0) const
	{
		if (std::abs(a - b) <= tol)
			return true;
		return false;
	}

	ResultObject processChildren(const NodeGeometries &children) const override
	{
		data = read_png_or_dat(filename);

		PolySet *p = new PolySet(3);
		p->setConvexity(convexity);

		for (img_data_t::iterator it = data.begin(); it != data.end(); it++) {
			lines = std::max(lines, (*it).first.first + 1);
			columns = std::max(columns, (*it).first.second + 1);
			min_val = std::min((*it).second - 1, min_val);
		}

		// Z top - X/Z plane
		for (int i = 1; i < lines; i++)
			for (int j = 1; j < columns; j++)
			{
				Vector3d top1, top2, top3, top4, topM;
				Vector3d bot1, bot2, bot3, bot4, botM;

				double v1 = getVecs(j - 1, i - 1, top1, bot1);
				double v2 = getVecs(j, i - 1, top2, bot2);
				double v3 = getVecs(j - 1, i, top3, bot3);
				double v4 = getVecs(j, i, top4, bot4);
				double m = (v1 + v2 + v3 + v4) / 4;
				topM = (top1 + top2 + top3 + top4) / 4;
				botM = (bot1 + bot2 + bot3 + bot4) / 4;

				if (vertCorners && m != 0 && !almost(m, 1)) {
					// bottom verts
					// if 3 adjacent verts are 0, move that corner
					if (v1 == 0 && v2 == 0 && v4 == 0)
						bot2 = top2 = (botM + topM) / 2;
					if (v2 == 0 && v4 == 0 && v3 == 0)
						bot4 = top4 = (botM + topM) / 2;
					if (v4 == 0 && v3 == 0 && v1 == 0)
						bot3 = top3 = (botM + topM) / 2;
					if (v3 == 0 && v1 == 0 && v2 == 0)
						bot1 = top1 = (botM + topM) / 2;

					// top verts
					// if 3 adjacent verts are 1, move the midpoint
					/*
					if (almost(v1, 1) && almost(v2, 1) && almost(v4, 1))
						topM[2] = height;
					if (almost(v2, 1) && almost(v4, 1) && almost(v3, 1))
						topM[2] = height;
					if (almost(v4, 1) && almost(v3, 1) && almost(v1, 1))
						topM[2] = height;
					if (almost(v3, 1) && almost(v1, 1) && almost(v2, 1))
						topM[2] = height;
						*/
				}

				if (!nonZero || m != 0) {
					p->append_poly();
					p->append_vertex(top1);
					p->append_vertex(top2);
					p->append_vertex(topM);
					if (nonZero && !invert) {
						p->append_poly();
						p->append_vertex(bot1);
						p->append_vertex(botM);
						p->append_vertex(bot2);
					}
				}

				if (!nonZero || m != 0) {
					p->append_poly();
					p->append_vertex(top2);
					p->append_vertex(top4);
					p->append_vertex(topM);
					if (nonZero && !invert) {
						p->append_poly();
						p->append_vertex(bot2);
						p->append_vertex(botM);
						p->append_vertex(bot4);
					}
				}

				if (!nonZero || m != 0) {
					p->append_poly();
					p->append_vertex(top4);
					p->append_vertex(top3);
					p->append_vertex(topM);
					if (nonZero && !invert) {
						p->append_poly();
						p->append_vertex(bot4);
						p->append_vertex(botM);
						p->append_vertex(bot3);
					}
				}

				if (!nonZero || m != 0) {
					p->append_poly();
					p->append_vertex(top3);
					p->append_vertex(top1);
					p->append_vertex(topM);
					if (nonZero && !invert) {
						p->append_poly();
						p->append_vertex(bot3);
						p->append_vertex(botM);
						p->append_vertex(bot1);
					}
				}
			}

		// X vertical sides - Y/Z plane
		for (int i = 1; i < lines; i++)
		{
			Vector3d top1, top2, top3, top4;
			Vector3d bot1, bot2, bot3, bot4;

			double v1 = getVecs(0, i - 1, top1, bot1);
			double v2 = getVecs(0, i, top2, bot2);
			double v3 = getVecs(columns - 1, i - 1, top3, bot3);
			double v4 = getVecs(columns - 1, i, top4, bot4);
			double m = (v1 + v2 + v3 + v4) / 4;

			if (!nonZero || m != 0) {
				p->append_poly();
				p->append_vertex(bot1);
				p->append_vertex(top1);
				p->append_vertex(bot2);

				p->append_poly();
				p->append_vertex(top1);
				p->append_vertex(top2);
				p->append_vertex(bot2);
			}

			if (!nonZero || m != 0) {
				p->append_poly();
				p->append_vertex(bot4);
				p->append_vertex(top4);
				p->append_vertex(bot3);

				p->append_poly();
				p->append_vertex(top4);
				p->append_vertex(top3);
				p->append_vertex(bot3);
			}
		}

		// Y vertical sides - X/Z plane
		for (int i = 1; i < columns; i++)
		{
			Vector3d top1, top2, top3, top4;
			Vector3d bot1, bot2, bot3, bot4;

			double v1 = getVecs(i - 1, 0, top1, bot1);
			double v2 = getVecs(i, 0, top2, bot2);
			double v3 = getVecs(i - 1, lines - 1, top3, bot3);
			double v4 = getVecs(i, lines - 1, top4, bot4);
			double m = (v1 + v2 + v3 + v4) / 4;

			if (!nonZero || m != 0) {
				p->append_poly();
				p->insert_vertex(bot1);
				p->insert_vertex(top1);
				p->insert_vertex(bot2);

				p->append_poly();
				p->insert_vertex(top1);
				p->insert_vertex(top2);
				p->insert_vertex(bot2);
			}

			if (!nonZero || m != 0) {
				p->append_poly();
				p->append_vertex(bot3);
				p->append_vertex(top3);
				p->append_vertex(bot4);

				p->append_poly();
				p->append_vertex(top3);
				p->append_vertex(top4);
				p->append_vertex(bot4);
			}
		}

		// Z bottom - X/Y plane
		if ((!nonZero || invert) && columns > 1 && lines > 1) {
			p->append_poly();
			double t = 0;
			for (int i = 0; i < columns - 1; i++)
				p->insert_vertex(getVec(i, 0, t));
			for (int i = 0; i < lines - 1; i++)
				p->insert_vertex(getVec(columns - 1, i, t));
			for (int i = columns - 1; i > 0; i--)
				p->insert_vertex(getVec(i, lines - 1, t));
			for (int i = lines - 1; i > 0; i--)
				p->insert_vertex(getVec(0, i, t));
		}

		return ResultObject(p);
	}
};

FactoryModule<SurfaceNode> SurfaceNodeFactory("surface");
