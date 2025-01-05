#!/bin/bash

set -e

sign_and_notarize () {

  local appBundle="$1"
  codesign --deep -o runtime -s "$IDENTITY" "${appBundle}"
  echo "Signed final .dmg"

  cat << EOF > gon.json
{
  "notarize": [{
    "path": "${appBundle}",
    "bundle_id": "mudletbootstrap",
    "staple": true
  }]
}
EOF

  for i in {1..3}; do
    echo "Trying to notarize (attempt ${i})"
    if gon gon.json; then
      break
    fi
  done

}


SOURCE_DIR="${GITHUB_WORKSPACE}"


# get commit date now before we check out an change into another git repository
COMMIT_DATE=$(git show -s --pretty="tformat:%cI" | cut -d'T' -f1 | tr -d '-')
YESTERDAY_DATE=$(date -v-1d '+%F' | tr -d '-')

#git clone https://github.com/Mudlet/installers.git "${BUILD_DIR}/../installers"

#cd "${BUILD_DIR}/../installers/osx"

# setup macOS keychain for code signing on ptb/release builds.
#if [ -n "$MACOS_SIGNING_PASS" ]; then
#    KEYCHAIN=build.keychain
#    security create-keychain -p travis $KEYCHAIN
#    security default-keychain -s $KEYCHAIN
#    security unlock-keychain -p travis $KEYCHAIN
#    security set-keychain-settings -t 3600 -u $KEYCHAIN
#    security import Certificates.p12 -k $KEYCHAIN -P "$MACOS_SIGNING_PASS" -T /usr/bin/codesign
#    security set-key-partition-list -S apple-tool:,apple: -s -k travis $KEYCHAIN
#    export IDENTITY="Developer ID Application"
#    echo "Imported identity:"
#    security find-identity
#    echo "----"
#fi


#if [ -n "${GITHUB_REPOSITORY}" ]; then
#    mv "${BUILD_DIR}/mudletbootstrap.app" "${BUILD_DIR}/${appBaseName}.app"
#else
#    mv "${BUILD_DIR}/MudletBootstrap.app" "${BUILD_DIR}/${appBaseName}.app"
#fi

#./make-installer.sh "${appBaseName}.app"

# Set HOMEBREW_PREFIX, HOMEBREW_CELLAR, add correct paths to PATH
eval "$(brew shellenv)"

# QT_ROOT_DIR set by install-qt action.
if [ -n "$QT_ROOT_DIR" ]; then
    QT_DIR="$QT_ROOT_DIR"
else
    # Check if QT_DIR is already set
    if [ -z "$QT_DIR" ]; then
        echo "QT_DIR not set."
        exit 1
    fi
fi

# Check if macdeployqt is in the path
if ! command -v macdeployqt &> /dev/null
then
    echo "Error: macdeployqt could not be found in the PATH."
    exit 1
fi

if [ ! -f "macdeployqtfix.py" ]; then
  wget https://raw.githubusercontent.com/arl/macdeployqtfix/master/macdeployqtfix.py
fi

echo "macdeployqtfix downloaded to $(pwd)"

npm install -g appdmg

mkdir -p "${GITHUB_WORKSPACE}/upload/"

while IFS= read -r line || [[ -n "$line" ]]; do
  gameName=$(echo "$line" | tr -cd '[:alnum:]_-')
  appBaseName="MudletBootstrap"
  BUILD_DIR="${GITHUB_WORKSPACE}/build-${gameName}"

  cd "${BUILD_DIR}"

  # get the app to package
  app=$(basename "${appBaseName}.app")

  if [ -z "$app" ]; then
    echo "No MudletBootstrap app folder to package given."
    echo "Usage: $pgm <MudletBootstrap app folder to package>"
    exit 2
  fi
  find . -iname "${app}" -type d
  app=$(find . -iname "${app}" -type d)
  if [ -z "${app}" ]; then
    echo "error: couldn't determine location of the ./app folder"
    echo "app = ${app}"

    exit 1
  fi

  echo "Deploying ${app}"

  # Bundle in Qt libraries
  echo "Running macdeployqt"
  macdeployqt "${app}" $( [ -n "$DEBUG" ] && echo "-verbose=3" )

  # fix unfinished deployment of macdeployqt
  echo "Running macdeployqtfix"
  python ${GITHUB_WORKSPACE}/macdeployqtfix.py "${app}/Contents/MacOS/MudletBootstrap" "${QT_DIR}" $( [ -n "$DEBUG" ] && echo "--verbose" )

  echo "Fixing plist entries..."
  /usr/libexec/PlistBuddy -c "Add CFBundleName string MudletBootstrap" "${app}/Contents/Info.plist" || true
  /usr/libexec/PlistBuddy -c "Add CFBundleDisplayName string MudletBootstrap" "${app}/Contents/Info.plist" || true

  /usr/libexec/PlistBuddy -c "Add CFBundleShortVersionString string ${shortVersion}" "${app}/Contents/Info.plist" || true
  /usr/libexec/PlistBuddy -c "Add CFBundleVersion string ${version}" "${app}/Contents/Info.plist" || true

  # Generate final .dmg
  cd ../../
  rm -f ~/Desktop/[mM]udletBootstrap-${gameName}*.dmg

  echo "PWD:"
  pwd
  echo "APP: ${app}"
  echo "BUILD_DIR: ${BUILD_DIR}"
  echo "SOURCE_DIR: ${SOURCE_DIR}"

  echo "Modifying config file..."
  cp ${SOURCE_DIR}/mudletbootstrap-appdmg.json ${BUILD_DIR}

  # Modify appdmg config file according to the app file to package
  perl -pi -e "s|../source/build/.*MudletBootstrap.*\\.app|${BUILD_DIR}/${app}|i" "${BUILD_DIR}/mudletbootstrap-appdmg.json"
  # Update icons to the correct type
  perl -pi -e "s|../source/src/icons/.*\\.icns|${SOURCE_DIR}/mudlet.icns|i" "${BUILD_DIR}/mudletbootstrap-appdmg.json"

  echo "Listing config file:"
  cat ${BUILD_DIR}/mudletbootstrap-appdmg.json

  echo "Creating appdmg..."
  # Last: build *.dmg file
  appdmg "${BUILD_DIR}/mudletbootstrap-appdmg.json" "${HOME}/Desktop/$(basename "${app%.*}").dmg"

  if [ -n "$MACOS_SIGNING_PASS" ]; then
      sign_and_notarize "${HOME}/Desktop/${appBaseName}.dmg"
  fi

  mv "${HOME}/Desktop/${appBaseName}.dmg" "${GITHUB_WORKSPACE}/upload/${appBaseName}-${gameName}.dmg"

done < "${GITHUB_WORKSPACE}/GameList.txt"

{
    echo "FOLDER_TO_UPLOAD=${GITHUB_WORKSPACE}/upload"
    echo "UPLOAD_FILENAME=${appBaseName}-macOS"
} >> "$GITHUB_ENV"
DEPLOY_URL="Github artifact, see https://github.com/$GITHUB_REPOSITORY/actions/runs/$GITHUB_RUN_ID"

# delete keychain just in case
if [ ! -z "$MACOS_SIGNING_PASS" ]; then
    security delete-keychain $KEYCHAIN
fi

export DEPLOY_URL