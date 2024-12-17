#!/bin/bash

# set -e
set -x

SOURCE_DIR="${GITHUB_WORKSPACE}"

ln -s "${BUILD_DIR}" source

# unset LD_LIBRARY_PATH as it upsets linuxdeployqt
export LD_LIBRARY_PATH=

echo "== Creating an AppImage =="

# setup linuxdeployqt binary if not found
if [ "$(getconf LONG_BIT)" = "64" ]; then
  if [[ ! -e linuxdeployqt.AppImage ]]; then
      # download prepackaged linuxdeployqt. Doesn't seem to have a "latest" url yet
      echo "linuxdeployqt not found - downloading one."
      wget --quiet -O linuxdeployqt.AppImage https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage
      chmod +x linuxdeployqt.AppImage
  fi
else
  echo "32bit Linux is currently not supported by the AppImage."
  exit 2
fi

cp "$SOURCE_DIR"/mudlet{.desktop,.png,.svg} build/

./linuxdeployqt.AppImage --appimage-extract

# Bundle libssl.so so Mudlet works on platforms that only distribute
# OpenSSL 1.1
cp -L /usr/lib/x86_64-linux-gnu/libssl.so* \
      build/lib/ || true
cp -L /lib/x86_64-linux-gnu/libssl.so* \
      build/lib/ || true
if [ -z "$(ls build/lib/libssl.so*)" ]; then
  echo "No OpenSSL libraries to copy found. Aborting..."
fi

./squashfs-root/AppRun ./build/MudletBootstrap -appimage

pwd
ls -R

chmod +x "MudletBootstrap.AppImage"
tar -cvf "MudletBootstrap-linux-x64.AppImage.tar" "MudletBootstrap.AppImage"

echo "=== ... later, via Github ==="
# Move the finished file into a folder of its own, because we ask Github to upload contents of a folder
mkdir "upload/"
mv "MudletBootstrap-linux-x64.AppImage.tar" "upload/"
{
  echo "FOLDER_TO_UPLOAD=$(pwd)/upload"
  echo "UPLOAD_FILENAME=MudletBootstrap-linux-x64"
} >> "$GITHUB_ENV"
DEPLOY_URL="Github artifact, see https://github.com/$GITHUB_REPOSITORY/runs/$GITHUB_RUN_ID"

export DEPLOY_URL

