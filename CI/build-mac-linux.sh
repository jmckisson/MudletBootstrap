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

RUNNER_WORKSPACE_UNIX_PATH=$(echo "${RUNNER_WORKSPACE}" | sed 's|\\|/|g' | sed 's|D:|/d|g')
export CCACHE_DIR=${RUNNER_WORKSPACE_UNIX_PATH}/ccache

echo "CCACHE_DIR is: ${CCACHE_DIR}"
echo "PATH is now:"
echo "${PATH}"
echo ""

cd $GITHUB_WORKSPACE || exit 1

LAUNCH_INI_PATH="${GITHUB_WORKSPACE}/resources/launch.ini"
echo "CMAKE_PREFIX_PATH is: ${CMAKE_PREFIX_PATH}"

echo "Building apps in GameList..."
while IFS= read -r line || [[ -n "$line" ]]; do
  gameName=$(echo "$line" | tr -cd '[:alnum:]_-')

  mkdir build-${gameName}
  cd build-${gameName}

  # Update the `launch.ini` file
  echo "Updating ${LAUNCH_INI_PATH} for MUDLET_PROFILES=${gameName}..."
  sed -i.bak "s/^MUDLET_PROFILES=.*/MUDLET_PROFILES=${gameName}/" "$LAUNCH_INI_PATH"

  echo "Running CMake configure..."

  echo "cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .."
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..

  echo "Building.."
  ninja

  echo " ${gameName} ... build finished"
  cd "$GITHUB_WORKSPACE" || exit 1

done < "${GITHUB_WORKSPACE}/GameList.txt"

cd ~ || exit 1
exit 0