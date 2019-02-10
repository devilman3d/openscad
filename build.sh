#!/bin/bash

setenv_linux_clang()
{
	export CC=clang
	export CXX=clang++
	export LINK=clang++
	export QMAKESPEC=linux-clang

	# format messages for Visual Studio -- only works with clang
	QCONFIG+=" CONFIG+=msvc_errors"
	# disable stupid clang notes
	QCONFIG+=" CONFIG+=nonotes"
	
	echo CC has been modified: $CC
	echo CXX has been modified: $CXX
	echo LINK has been modified: $LINK
	echo QMAKESPEC has been modified: $QMAKESPEC
}

# exit on any error
#set -e
# echo commands in this scropt
#set -x
# fail on any pipe error with that error
set -o pipefail

echo Working directory: `pwd`

QCONFIG="CONFIG+=release CONFIG-=debug"
if [ $1 = "Debug" ]
then
	QCONFIG="CONFIG-=release CONFIG+=debug"
fi

# set environment to use clang
setenv_linux_clang

# less output noise during the build
QCONFIG+=" CONFIG+=quiet"
# use precompiled headers
QCONFIG+=" CONFIG+=precompile_header"
# enable experimental OpenSCAD features
QCONFIG+=" CONFIG+=experimental"
# do a dev build
QCONFIG+=" CONFIG+=devbuild"

echo Current environment:
export

echo Running qmake with: $QCONFIG
qmake $QCONFIG -o Makefile.$1 openscad.pro
export ERRORLEVEL=$?
if [ $ERRORLEVEL -ne 0 ]
then
	echo Error running qmake to generate Makefile.$1: $ERRORLEVEL
	echo $ERRORLEVEL > BuildResult.$1
	exit $ERRORLEVEL
fi
echo Generated Makefile.$1

if [ $# -gt 1 ]
then
	if [ $2 = "Clean" ]
	then
		echo Cleaning: Makefile.$1
		make -f Makefile.$1 clean 2>&1
		export ERRORLEVEL=$?
		exit $ERRORLEVEL
	fi
fi

echo Checking for changes
make -q -f Makefile.$1
export ERRORLEVEL=$?
if [ $ERRORLEVEL -eq 0 ]
then
	echo "Nothing to do..."
	exit $ERRORLEVEL
fi

#echo Touching parser.y and lexer.l
#rm -f objects/*.cxx

# set some flags for make
MAKE_MP="-j3"

make $MAKE_MP -f Makefile.$1 2>&1
# 2>&1 | sed -u -r 's/:([0-9]+):([0-9]+): ([a-zA-Z0-9 ]+):/(\1, \2): \3 :/;'
#| sed -u -r 's/:([0-9]+):([0-9]+): (warning|error):/(\1, \2): \3 :/;s/:([0-9]+):([0-9]+)([:,]+)/(\1, \2)\3/;s/:([0-9]+)([:,]+)/(\1)\2/;s/\/[a-z\/]+src\//src\//;s/ -c.* -o / /;'

export ERRORLEVEL=$?
exit $ERRORLEVEL
