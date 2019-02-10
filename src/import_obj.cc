#include "import.h"
#include "polyset.h"
#include "handle_dep.h" // handle_dep()
#include "printutils.h"

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>

std::vector<PolySet*> import_obj(const std::string &filename)
{
	std::vector<PolySet*> result;

	handle_dep(filename);
	// Open file and position at the end
	std::ifstream f(filename.c_str(), std::ios::in);
	if (!f.good()) {
		PRINTB("WARNING: Can't open import file '%s'.", filename);
		return result;
	}

	boost::regex ex_obj("\\s*#\\s+object\\s+([^\\s]+)");
	boost::regex ex_group("\\s*g\\s+([^\\s]+)");
	boost::regex ex_vertex("\\s*v\\s+([^\\s]+)\\s+([^\\s]+)\\s+([^\\s]+)");
	std::string faceIdxStr = "\\s+([\\d]+)/([\\d]*)/([\\d]*)";
	boost::regex ex_face("\\s*f" + faceIdxStr + faceIdxStr + faceIdxStr);

	Grid3d<size_t> grid(GRID_FINE);
	std::vector<Vector3d> vertices;
	std::vector<std::string> vlines;
	PolySet *p = nullptr;
	std::string obj;
	std::string group;
	std::string line;
	std::getline(f, line);
	while (!f.eof()) {
		boost::trim(line);
		boost::smatch results;
		if (boost::regex_search(line, results, ex_obj)) {
			obj = results[1];
			PRINTB("Object: %s", obj);
		}
		else if (boost::regex_search(line, results, ex_group)) {
			if (p != nullptr) {
				//PRINTB("Validating PolySet: %s", group);
				//p->validate();
			}
			group = results[1];
			PRINTB("Creating PolySet: %s", group);
			p = new PolySet(3);
			result.push_back(p);
			grid = Grid3d<size_t>(GRID_FINE);
		}
		else if (boost::regex_search(line, results, ex_vertex)) {
			Vector3d vdata;
			try {
				for (int v = 0; v < 3; v++) {
					vdata[v] = boost::lexical_cast<double>(results[v + 1]);
				}
			}
			catch (const boost::bad_lexical_cast &blc) {
				PRINTB("WARNING: Can't parse vertex line '%s'.", line);
			}
			grid.align(vdata);
			vertices.push_back(vdata);
			vlines.push_back(line);
		}
		else if (boost::regex_search(line, results, ex_face)) {
			int idata[3] = { 0 };
			try {
				for (int i = 0; i < 3; ++i) {
					idata[i] = boost::lexical_cast<int>(results[i * 3 + 1]) - 1;
				}
			}
			catch (const boost::bad_lexical_cast &blc) {
				PRINTB("WARNING: Can't parse face line '%s'.", line);
			}
			//if (vertices[idata[0]].squaredNorm() == 0 || vertices[idata[1]].squaredNorm() == 0 || vertices[idata[2]].squaredNorm() == 0) {
			//	PRINTB("Adding face with zero-point: %s", line);
			//	PRINTB("  %d: %s", idata[0] % vlines[idata[0]]);
			//	PRINTB("  %d: %s", idata[1] % vlines[idata[1]]);
			//	PRINTB("  %d: %s", idata[2] % vlines[idata[2]]);
			//}
			p->append_poly();
			p->append_vertex(vertices[idata[0]]);
			p->append_vertex(vertices[idata[1]]);
			p->append_vertex(vertices[idata[2]]);
		}
		std::getline(f, line);
	}

	if (p != nullptr) {
		//PRINTB("Validating PolySet: %s", group);
		//p->validate();
	}

	return result;
}
