#ifndef stdafx_hpp
#define stdafx_hpp

// undefine _GLIBCXX_DEBUG because it causes problems when
// mixing debug and release with precompiled headers
#pragma push_macro("_GLIBCXX_DEBUG")
#undef _GLIBCXX_DEBUG

// VisualStudio kludge: make it think it is Linux
#ifdef __INTELLISENSE__
#undef _WIN64
#undef WIN64
#define _linux_
#endif

// no defines in this file -- only include

// c includes here
#include <libintl.h>
#include <locale.h>
#include <time.h>
#include <stddef.h>

#ifdef __cplusplus

// c++ includes here
// stl
#include <string>
#include <algorithm>
#include <limits>
#include <functional>
#include <utility>
#include <vector>
#include <list>
#include <queue>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <ostream>
#include <sstream>
#include <iostream>
#include <memory>
#include <cstdint>
#include <cstring>

// boost
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>

// cgal
#ifdef ENABLE_CGAL
#include "cgal.h"
#endif

// project
#include "system-gl.h"

#ifndef __INTELLISENSE__

// compile-time includes here -- don't kill intellisense with them

#include <QString>
#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/Dense>

#endif // __INTELLISENSE__
#endif // __cplusplus

#pragma pop_macro("_GLIBCXX_DEBUG")

#endif // stdafx_hpp