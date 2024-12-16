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
#          1.5.0    No change
#          1.4.0    No change
#          1.3.0    Don't explicitly install the no longer supported QT 5
#                   Gamepad stuff (since PR #6787 was merged into
#                   development branch) - it may still be installed as part
#                   of a Qt5 installation but we don't use it any more.
#          1.2.0    Tweak luarocks --tree better and report on failure to
#                   complete
#          1.1.0    Updated to not do things that have already been done
#                   and to offer a choice between a base or a full install
#          1.0.0    Original version

# Script to run once in a ${GITHUB_WORKFLOW} directory in a MSYS2 shell to
# install as much as possible to be able to develop 64/32 Bit Windows
# version of Mudlet

# To be used prior to building Mudlet, after that run:
# * build-mudlet-for-window.sh to compile the currently checked out code
# * package-mudlet-for-windows.sh to put everything together in an archive that
#   will be deployed from a github workflow

# Exit codes:
# 0 - Everything is fine. 8-)
# 1 - Failure to change to a directory
# 2 - Unsupported MSYS2/MINGGW shell type
# 5 - Invalid command line argument
# 6 - One or more Luarocks could not be installed
# 7 - One of more packages failed to install


if [ "${MSYSTEM}" = "MINGW64" ]; then
  export BUILD_BITNESS="64"
  export BUILDCOMPONENT="x86_64"
elif [ "${MSYSTEM}" = "MSYS" ]; then
  echo "Please run this script from an MINGW32 or MINGW64 type bash terminal appropriate"
  echo "to the bitness you want to work on. You may do this once for each of them should"
  echo "you wish to do both."
  exit 2
else
  echo "This script is not set up to handle systems of type ${MSYSTEM}, only MINGW32 or"
  echo "MINGW64 are currently supported. Please rerun this in a bash terminal of one"
  echo "of those two types."
  exit 2
fi

# We use this internally - but it is actually the same as ${MINGW_PREFIX}
export MINGW_BASE_DIR=$MSYSTEM_PREFIX
# A more compact - but not necessarily understood by other than MSYS/MINGW
# executables - path:
export MINGW_INTERNAL_BASE_DIR="/mingw${BUILD_BITNESS}"
#
# FIXME: don't add duplicates but rearrange instead to put them in the "right" order:
#
export PATH="${MINGW_INTERNAL_BASE_DIR}/usr/local/bin:${MINGW_INTERNAL_BASE_DIR}/bin:/usr/bin:${PATH}"
echo "MSYSTEM is: ${MSYSTEM}"
echo "PATH is now: ${PATH}"
echo ""

# Options to consider:
# --Sy = Sync, refresh as well as installing the specified packages
# --noconfirm = do not ask for user intervention
# --noprogressbar = do not show progress bars as they are not useful in scripts
echo "  Updating and installing ${MSYSTEM} packages..."
echo ""
echo "    This could take a long time if it is needed to fetch everything, so feel free"
echo "    to go and have a cup of tea (other beverages are available) in the meantime...!"
echo ""


#echo "=== Installing Qt6 Packages ==="
#pacman_attempts=1
#while true; do
#    if /usr/bin/pacman -Su --needed --noconfirm \
#        "mingw-w64-${BUILDCOMPONENT}-qt6-base" \
#        "mingw-w64-${BUILDCOMPONENT}-qt6-tools"; then
#        break
#    fi

#    if [ $pacman_attempts -eq 10 ]; then
#        exit 7
#    fi
#    pacman_attempts=$((pacman_attempts +1))

#    echo "=== Some packages failed to install, waiting and trying again ==="
#    sleep 10
#done


pacman_attempts=1
while true; do
  if /usr/bin/pacman -Su --needed --noconfirm \
    git \
    man \
    rsync \
    python \
    perl \
    bison \
    flex \
    "mingw-w64-${BUILDCOMPONENT}-ccache" \
    "mingw-w64-${BUILDCOMPONENT}-ntldd" \
    "mingw-w64-${BUILDCOMPONENT}-toolchain" \
    "mingw-w64-${BUILDCOMPONENT}-zlib" \
    "mingw-w64-${BUILDCOMPONENT}-icu" \
    "mingw-w64-${BUILDCOMPONENT}-openssl" \
    "mingw-w64-${BUILDCOMPONENT}-cmake" \
    "mingw-w64-${BUILDCOMPONENT}-ninja"; then
      break
  fi

  if [ $pacman_attempts -eq 10 ]; then
    exit 7
  fi
  pacman_attempts=$((pacman_attempts +1))

  echo "=== Some packages failed to install, waiting and trying again ==="
  sleep 10
done

#export CC="ccache gcc"
#export CXX="ccache g++"
ccache --max-size=10G

echo "=== Listing Environment Variables ==="
printenv

exit 0