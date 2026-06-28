cmake_minimum_required(VERSION 3.16)
project(Vela VERSION 1.0.0 LANGUAGES CXX
        DESCRIPTION "A VLC-inspired media player powered by libmpv/FFmpeg")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt code generation (moc/rcc/uic) is handled automatically.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# Locate libmpv. pkg-config is used only for hints; the real header and
# library are resolved with find_path/find_library. This avoids a known
# MSYS2/UCRT64 issue where pkg-config reports MSYS-style paths such as
# "/ucrt64/include" that break CMake's IMPORTED_TARGET path validation.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_MPV QUIET mpv)
endif()

find_path(MPV_INCLUDE_DIR
    NAMES mpv/client.h
    HINTS ${PC_MPV_INCLUDEDIR} ${PC_MPV_INCLUDE_DIRS}
)
find_library(MPV_LIBRARY
    NAMES mpv libmpv mpv-2 libmpv-2 libmpv.dll
    HINTS ${PC_MPV_LIBDIR} ${PC_MPV_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MPV
    REQUIRED_VARS MPV_LIBRARY MPV_INCLUDE_DIR)

qt_standard_project_setup()

qt_add_executable(vela
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/MpvWidget.cpp
    src/MpvWidget.h
    src/PlayerControls.cpp
    src/PlayerControls.h
    src/SubtitleManager.cpp
    src/SubtitleManager.h
    src/SubtitleDialogs.cpp
    src/SubtitleDialogs.h
    resources/resources.qrc
)

target_include_directories(vela PRIVATE src ${MPV_INCLUDE_DIR})
target_link_libraries(vela PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network ${MPV_LIBRARY})

# ---- Installation (lets you set Vela as the system default video player) ----
include(GNUInstallDirs)
install(TARGETS vela RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/vela.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES resources/icons/vela.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
