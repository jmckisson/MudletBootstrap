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

# Exit codes:
# 0 - Everything is fine. 8-)
# 1 - Failure to change to a directory
# 2 - Unsupported fork
# 3 - Not used
# 4 - nuget error
# 5 - squirrel error

if [ "${MSYSTEM}" = "MSYS" ]; then
  echo "Please run this script from an MINGW32 or MINGW64 type bash terminal appropriate"
  echo "to the bitness you want to work on. You may do this once for each of them should"
  echo "you wish to do both."
  exit 2
elif [ "${MSYSTEM}" = "MINGW64" ]; then
  export BUILD_BITNESS="64"
  export BUILDCOMPONENT="x86_64"
  export ARCH="x86"
else
  echo "This script is not set up to handle systems of type ${MSYSTEM}, only MINGW32 or"
  echo "MINGW64 are currently supported. Please rerun this in a bash terminal of one"
  echo "of those two types."
  exit 2
fi

cd "$GITHUB_WORKSPACE" || exit 1

GITHUB_WORKSPACE_UNIX_PATH=$(echo "${GITHUB_WORKSPACE}" | sed 's|\\|/|g' | sed 's|D:|/d|g')

echo "=== Setting up upload directory ==="
uploadDir="${GITHUB_WORKSPACE}\\upload"
uploadDirUnix=$(echo "${uploadDir}" | sed 's|\\|/|g' | sed 's|D:|/d|g')

# Check if the upload directory exists, if not, create it
if [[ ! -d "$uploadDirUnix" ]]; then
  mkdir -p "$uploadDirUnix"
fi

while IFS= read -r line || [[ -n "$line" ]]; do

  gameName=$(echo "$line" | tr -cd '[:alnum:]_-')

  PACKAGE_DIR="${GITHUB_WORKSPACE_UNIX_PATH}/package-${gameName}"

  cd "$PACKAGE_DIR" || exit 1

  # Remove specific file types from the directory
  rm ./*.cpp ./*.o

  mv "$PACKAGE_DIR/MudletBootstrap.exe" "MudletBootstrap-${gameName}.exe"

  # Move packaged files to the upload directory
  echo "=== Copying files to upload directory ==="
  rsync -avR "${PACKAGE_DIR}"/./* "$uploadDirUnix"

  cd "$GITHUB_WORKSPACE" || exit 1

done < "${GITHUB_WORKSPACE}/GameList.txt"

# Append these variables to the GITHUB_ENV to make them available in subsequent steps
{
  echo "FOLDER_TO_UPLOAD=${uploadDir}\\"
  echo "UPLOAD_FILENAME=MudletBootstrap-${MSYSTEM}"
} >> "$GITHUB_ENV"
