#!/bin/bash

echo "=== Cloning Qt Source Repository ==="
cd ${RUNNER_WORKSPACE}
git clone --branch 6.8.1 --depth 1 --no-recurse-submodules https://github.com/qt/qt5.git qt6-source
cd qt6-source
git submodule update --init qtbase qttools qttranslations

echo "=== Configuring Qt for Static Linking ==="
perl init-repository --module-subset=qtbase,qttools,qttranslations
cd ${RUNNER_WORKSPACE}
mkdir qt-static-build
cd qt-static-build
export CMAKE_SUPPRESS_DEVELOPER_WARNINGS=ON

../qt6-source/configure -prefix ${RUNNER_WORKSPACE}/qt-static-install -static -static-runtime -release -opensource -no-shared -confirm-license -init-submodules -submodules qtbase,qttranslations,qttools -nomake tests -nomake examples -skip qt3d -skip qtmultimedia -skip qtdeclarative -skip qtshadertools -skip qtquick -skip designer -no-opengl -no-dbus -platform win32-g++ -openssl-linked

# CMake configuration with ccache integration
#cmake -DCMAKE_BUILD_TYPE=Release \
#      -DCMAKE_INSTALL_PREFIX=$PWD/qt-static-install \
#      -DCMAKE_C_COMPILER_LAUNCHER=ccache \
#      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
#      -DQT_BUILD_TESTS=OFF \
#      -DQT_BUILD_EXAMPLES=OFF \
#      -DBUILD_SHARED_LIBS=OFF \
#      -G Ninja ../qt6-source

echo "=== Compiling Qt ==="
cmake --build . --parallel

echo "=== Installing Qt ==="
cmake --install .