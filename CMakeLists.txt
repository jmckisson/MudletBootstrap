cmake_minimum_required(VERSION 3.25)

project(MudletBootstrap VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(QT_STATIC ON)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

    # Explicitly find static libb2
    find_library(LIBB2_STATIC libb2.a REQUIRED)
    find_library(LIB_BROTLIDEC libbrotlidec.a REQUIRED)
    find_library(LIB_BROTLICOMMON libbrotlicommon.a REQUIRED)
    find_library(LIB_ZLIB libz.a REQUIRED)
    find_library(LIB_ZSTD libzstd.a REQUIRED)

    # Enable static runtime linking (GCC/MinGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -Wl,--gc-sections")
endif()

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets LinguistTools Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets LinguistTools Network)

set(TS_FILES MudletBootstrap_en_US.ts)

set(PROJECT_SOURCES
        main.cpp
        MudletBootstrap.h
        MudletBootstrap.cpp
        ${TS_FILES}
)

qt_add_executable(MudletBootstrap
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_link_libraries(MudletBootstrap PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

if(WIN32)
    target_link_libraries(MudletBootstrap PRIVATE
        ${LIBB2_STATIC}
        ${LIB_BROTLIDEC}
        ${LIB_BROTLICOMMON}
        ${LIB_ZLIB}
        ${LIB_ZSTD}
    )
endif()

qt_add_resources(RESOURCES resources.qrc)
target_sources(MudletBootstrap PRIVATE ${RESOURCES})


set_target_properties(MudletBootstrap PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS MudletBootstrap
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MudletBootstrap)
endif()
