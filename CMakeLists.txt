cmake_minimum_required(VERSION 3.16)
project(wl-dock VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(QT_MIN_VERSION 6.4)
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Widgets DBus)
find_package(LayerShellQt REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client)
find_program(WAYLAND_SCANNER NAMES wayland-scanner REQUIRED)
set(PROTO_XML ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-foreign-toplevel-management-unstable-v1.xml)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

set(GEN_HEADER ${GEN_DIR}/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h)
set(GEN_SOURCE ${GEN_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.c)

add_custom_command(
    OUTPUT ${GEN_HEADER}
    COMMAND ${WAYLAND_SCANNER} client-header ${PROTO_XML} ${GEN_HEADER}
    DEPENDS ${PROTO_XML}
    VERBATIM
)
add_custom_command(
    OUTPUT ${GEN_SOURCE}
    COMMAND ${WAYLAND_SCANNER} private-code ${PROTO_XML} ${GEN_SOURCE}
    DEPENDS ${PROTO_XML}
    VERBATIM
)

set(SOURCES
    src/main.cpp
    src/ForeignToplevelManager.cpp
    src/DesktopIconResolver.cpp
    src/TaskButton.cpp
    src/TaskManager.cpp
    src/ClockWidget.cpp
    src/DockWindow.cpp
    src/DockController.cpp
    src/DockTip.cpp
    src/sni/SniItem.cpp
    src/sni/SniWatcher.cpp
    src/sni/SniIconButton.cpp
    src/sni/SniTrayWidget.cpp
    src/sni/SniDbusMenu.cpp
    src/sni/SniMenu.cpp
    ${GEN_SOURCE}
)

set(HEADERS
    src/ForeignToplevelManager.h
    src/DesktopIconResolver.h
    src/TaskButton.h
    src/TaskManager.h
    src/ClockWidget.h
    src/DockWindow.h
    src/DockController.h
    src/DockTip.h
    src/sni/SniTypes.h
    src/sni/SniItem.h
    src/sni/SniWatcher.h
    src/sni/SniIconButton.h
    src/sni/SniTrayWidget.h
    src/sni/SniDbusMenu.h
    src/sni/SniMenu.h
)

add_executable(wl-dock ${SOURCES} ${HEADERS} ${GEN_HEADER})
target_include_directories(wl-dock PRIVATE ${GEN_DIR} src)
target_compile_definitions(wl-dock PRIVATE WL_DOCK_VERSION="${PROJECT_VERSION}")
target_link_libraries(wl-dock PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    LayerShellQt::Interface
    PkgConfig::WAYLAND
)
