#!/bin/bash
###########################################################################
#   Copyright (C) 2024-2024  by John McKisson - john.mckisson@gmail.com   #
#   Copyright (C) 2023-2024  by Stephen Lyons - slysven@virginmedia.com   #
#                                                                         #
#   This program is free software; you can redistribute it and/or modify  #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 2 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   This program is distributed in the hope that it will be useful,       #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program; if not, write to the                         #
#   Free Software Foundation, Inc.,                                       #
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             #
###########################################################################

# Version: 2.0.0    Rework to build on an MSYS2 MINGW64 Github workflow
#          1.5.0    Change BUILD_TYPE to BUILD_CONFIG to avoid clash with
#                   CI/CB system using same variable
#          1.4.0    No change
#          1.3.0    Remove used of the no longer supported/used by us QT5
#                   Gamepad stuff (since PR #6787 was merged into
#                   the development branch)
#          1.2.0    No changes
#          1.1.0    Updated to bail out if there isn't a mudlet.exe file to
#                   work with
#          1.0.0    Original version

# Script to each time to package all the files needed to run Mudlet on
# Windows in a archive file that will be deployed from a github workflow

# To be used AFTER setup-windows-sdk.sh and build-mudlet-for-windows.sh
# have been run.

# Exit codes:
# 0 - Everything is fine. 8-)
# 1 - Failure to change to a directory
# 2 - Unsupported MSYS2/MINGGW shell type
# 3 - Unsupported build type
# 4 - Directory to be used to assemble the package is NOT empty
# 6 - No Mudlet.exe file found to work with

if [ "${MSYSTEM}" = "MSYS" ]; then
  echo "Please run this script from an MINGW32 or MINGW64 type bash terminal appropriate"
  echo "to the bitness you want to work on. You may do this once for each of them should"
  echo "you wish to do both."
  exit 2
elif [ "${MSYSTEM}" = "MINGW64" ]; then
  export BUILD_BITNESS="64"
else
  echo "This script is not set up to handle systems of type ${MSYSTEM}, only MINGW32 or"
  echo "MINGW64 are currently supported. Please rerun this in a bash terminal of one"
  echo "of those two types."
  exit 2
fi

BUILD_CONFIG="release"
MINGW_INTERNAL_BASE_DIR="/mingw${BUILD_BITNESS}"
export MINGW_INTERNAL_BASE_DIR
GITHUB_WORKSPACE_UNIX_PATH=$(echo ${GITHUB_WORKSPACE} | sed 's|\\|/|g' | sed 's|D:|/d|g')

echo "MSYSTEM is: ${MSYSTEM}"
echo ""

cd $GITHUB_WORKSPACE_UNIX_PATH || exit 1

#echo "Finding windeployqt6.."
#find ${RUNNER_WORKSPACE}/qt-static-install | grep windeployqt6

while IFS= read -r line || [[ -n "$line" ]]; do

  gameName=$(echo "$line" | tr -cd '[:alnum:]_-')

  PACKAGE_DIR="${GITHUB_WORKSPACE_UNIX_PATH}/package-${gameName}"

  if [ -d "${PACKAGE_DIR}" ]; then
    # The wanted packaging dir exists - as is wanted
    echo ""
    echo "Checking for an empty ${PACKAGE_DIR} in which to assemble files..."
    echo ""
    if [ -n "$(ls -A ${PACKAGE_DIR})" ]; then
      # But it isn't empty...
      echo "${PACKAGE_DIR} does not appear to be empty, please"
      echo "erase everything there and try again."
      exit 4
    fi
  else
    echo ""
    echo "Creating ${PACKAGE_DIR} in which to assemble files..."
    echo ""
    # This will create the directory if it doesn't exist but won't moan if it does
    mkdir -p "${PACKAGE_DIR}"
  fi
  cd "${PACKAGE_DIR}" || exit 1
  echo ""

  echo "Copying wanted compiled files from ${GITHUB_WORKSPACE}/build-${gameName} to ${GITHUB_WORKSPACE}/package-${gameName} ..."
  echo ""

  if [ ! -f "${GITHUB_WORKSPACE_UNIX_PATH}/build-${gameName}/MudletBootstrap.exe" ]; then
    echo "ERROR: no MudletBootstrap executable found - did the previous build"
    echo "complete sucessfully?"
    exit 6
  fi

  cp "${GITHUB_WORKSPACE_UNIX_PATH}/build-${gameName}/MudletBootstrap.exe" "${PACKAGE_DIR}/"
  if [ -f "${GITHUB_WORKSPACE_UNIX_PATH}/build-${gameName}/MudletBootstrap.exe.debug" ]; then
    cp "${GITHUB_WORKSPACE_UNIX_PATH}/build-${gameName}/MudletBootstrap.exe.debug" "${PACKAGE_DIR}/"
  fi


  "${RUNNER_WORKSPACE}/qt-static-install/bin/windeployqt6" ./MudletBootstrap.exe

  ZIP_FILE_NAME="MudletBootstrap"


  # To determine which system libraries have to be copied in it requires
  # continually trying to run the executable on the target type system
  # and adding in the libraries to the same directory and repeating that
  # until the executable actually starts to run. Alternatively running
  # ntldd ./mudlet.exe | grep "/mingw32" {for the 32 bit case, use "64" for
  # the other one} inside an Mingw32 (or 64) shell as appropriate will
  # produce the libraries that are likely to be needed below. Unfortunetly
  # this process is a little recursive in that you may have to repeat the
  # process for individual librarys. For ones used by lua modules this
  # can manifest as being unable to "require" the library within lua
  # and doing the above "ntldd" check revealed that, for instance,
  # "luasql/sqlite3.dll" needed "libsqlite3-0.dll"!
  #
  echo ""
  #echo "Examining MudletBootstrap application to identify other needed libraries..."

  #  NEEDED_LIBS=$("${MINGW_INTERNAL_BASE_DIR}/bin/ntldd" --recursive ./MudletBootstrap.exe \
  #    | /usr/bin/grep -v "Qt6" \
  #    | /usr/bin/grep -i "mingw" \
  #    | /usr/bin/cut -d ">" -f2 \
  #    | /usr/bin/cut -d "(" -f1 \
  #    | /usr/bin/sort)

  #echo ""
  #echo "Copying these identified libraries..."
  #for LIB in ${NEEDED_LIBS} ; do
  #  cp -v -p "${LIB}" . ;
  #done

  cd $GITHUB_WORKSPACE_UNIX_PATH || exit 1

done < "${GITHUB_WORKSPACE}/GameList.txt"

exit 0