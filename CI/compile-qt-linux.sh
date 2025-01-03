#!/bin/bash

echo "=== Cloning Qt Source Repository ==="
cd ${GITHUB_WORKSPACE}
git clone --branch 6.8.1 --depth 1 --no-recurse-submodules https://github.com/qt/qt5.git qt6-source
cd qt6-source
git submodule update --init qtbase qttranslations

echo "=== Configuring Qt for Static Linking ==="
#perl init-repository --module-subset=qtbase,qttranslations
cd ${GITHUB_WORKSPACE}
mkdir qt-static-build
cd qt-static-build
export CMAKE_SUPPRESS_DEVELOPER_WARNINGS=ON

${GITHUB_WORKSPACE}/qt6-source/configure -prefix ${GITHUB_WORKSPACE}/qt-static-install -static -release -opensource -confirm-license -init-submodules -submodules qtbase,qttranslations -nomake tests -nomake examples -skip qt3d -skip qtmultimedia -skip qtdeclarative -skip qtactiveqt  -skip qtshadertools -skip qtquick -skip designer -no-opengl -no-dbus -platform linux-g++ -openssl-linked -DCMAKE_PREFIX_PATH=/usr/lib/llvm-12

echo "=== Compiling Qt ==="
cmake --build . -- -j2

echo "=== Installing Qt ==="
cmake --install .