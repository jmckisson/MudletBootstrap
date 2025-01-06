cmake_minimum_required(VERSION 3.5)

project(MudletBootstrap VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(QT_STATIC ON)
    set(BUILD_SHARED_LIBS OFF)
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

if(WIN32)
# Add static Qt resource file
    qt_add_resources(RESOURCES resources.qrc)
    target_sources(MudletBootstrap PRIVATE ${RESOURCES})
endif()

target_link_libraries(MudletBootstrap PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

qt_add_resources(RESOURCES resources.qrc)
target_sources(MudletBootstrap PRIVATE ${RESOURCES})


set_target_properties(MudletBootstrap PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(WIN32)
    # Disable plugin dependencies for static build
    if(QT_VERSION_MAJOR EQUAL 6)
        qt_finalize_executable(MudletBootstrap)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS MudletBootstrap
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MudletBootstrap)
endif()
