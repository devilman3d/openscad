#include "AST.h"
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

Location Location::NONE("", 0, 0, 0, 0);

/*!
	Returns the absolute path to the given filename, unless it's empty.

	NB! This will actually search for the file, to be backwards compatible with <= 2013.01
	(see issue #217)
*/
std::string Location::getAbsolutePath(const std::string &filename) const
{
	if (!filename.empty() && !fs::path(filename).is_absolute()) {
		return fs::absolute(fs::path(this->file) / filename).string();
	}
	else {
		return filename;
	}
}
