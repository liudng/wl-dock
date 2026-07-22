cmake_minimum_required(VERSION 3.16)
project(wl-dock VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

set(QT_MIN_VERSION 6.4)
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Widgets DBus Network)
find_package(LayerShellQt REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client)
find_program(WAYLAND_SCANNER NAMES wayland-scanner REQUIRED)
set(PROTO_XML ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-foreign-toplevel-management-unstable-v1.xml)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

# === 静态检查与 Sanitizer 选项 ===
# 开启 clang-tidy：cmake -B build -D WL_DOCK_ENABLE_CLANG_TIDY=ON
option(WL_DOCK_ENABLE_CLANG_TIDY "Run clang-tidy together with the compiler" OFF)
if(WL_DOCK_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE
                 NAMES clang-tidy clang-tidy-19 clang-tidy-18 clang-tidy-17 clang-tidy-16)
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
        # 使用项目根目录的 .clang-tidy 作为配置文件
        set(CMAKE_CXX_CLANG_TIDY
            "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;--extra-arg=-Wno-unknown-warning-option")
    else()
        message(WARNING "WL_DOCK_ENABLE_CLANG_TIDY=ON but clang-tidy not found")
    endif()
endif()

# === CPack 包标识标签（用于发布文件命名）===
# 例：-DWL_DOCK_PACKAGE_TAG=debian-13-amd64 生成 wl-dock-0.5.0-debian-13-amd64.{tar.gz,deb,rpm}
set(WL_DOCK_PACKAGE_TAG "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
    CACHE STRING "Tag used in CPack package file names (e.g. debian-13-amd64)")

# 开启 ASan/LSan/UBSan：cmake -B build -D WL_DOCK_ENABLE_SANITIZERS=ON
# 运行时建议设置：ASAN_OPTIONS=detect_leaks=1:abort_on_error=0
option(WL_DOCK_ENABLE_SANITIZERS
       "Enable AddressSanitizer + LeakSanitizer + UndefinedBehaviorSanitizer" OFF)
if(WL_DOCK_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(WARNING "Sanitizers only supported on GCC/Clang, ignored")
    else()
        message(STATUS "Sanitizers enabled: ASan + LSan + UBSan")
        # 编译选项：ASan 默认开启 LSan；UBSan 捕获未定义行为；保留帧指针便于栈追踪
        add_compile_options(
            -fsanitize=address
            -fsanitize=undefined
            -fsanitize=leak
            -fno-omit-frame-pointer
            -fno-common
            -g
        )
        # 链接选项：必须与编译选项一致，让链接器解析 sanitizer 运行时
        add_link_options(
            -fsanitize=address
            -fsanitize=undefined
            -fsanitize=leak
        )
    endif()
endif()

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
    Qt6::Network
    LayerShellQt::Interface
    PkgConfig::WAYLAND
)

# === 安装规则 ===
# 可执行文件 → /usr/bin/wl-dock
install(TARGETS wl-dock
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# man 帮助页：构建时压缩为 .gz，安装到 /usr/share/man/man1/
# 找不到 gzip 时退化为安装未压缩的 .1 文件
find_program(GZIP_EXECUTABLE gzip)
if(GZIP_EXECUTABLE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/wl-dock.1.gz
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/wl-dock.1
                ${CMAKE_CURRENT_BINARY_DIR}/wl-dock.1
        COMMAND ${GZIP_EXECUTABLE} -n -9 -f wl-dock.1
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/wl-dock.1
        COMMENT "Compressing man page wl-dock.1"
        VERBATIM
    )
    add_custom_target(wl-dock-man ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/wl-dock.1.gz)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/wl-dock.1.gz
            DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
else()
    install(FILES wl-dock.1
            DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()

# .desktop 应用入口 → /usr/share/applications/
install(FILES wl-dock.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

# SVG 应用图标 → /usr/share/icons/hicolor/scalable/apps/
install(FILES wl-dock.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)

# README 文档 → /usr/share/doc/wl-dock/
install(FILES README.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR})

# === CPack 配置 ===
# 同时生成 TGZ / DEB / RPM，遵循 Linux FHS 目录结构
set(CPACK_PACKAGE_NAME "wl-dock")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Wayland dock for wlroots-based compositors")
set(CPACK_PACKAGE_DESCRIPTION
    "wl-dock is a lightweight dock for Wayland compositors implementing "
    "the wlr-layer-shell and wlr-foreign-toplevel-management protocols. "
    "It is primarily designed for Labwc, sway and Hyprland.")
set(CPACK_PACKAGE_CONTACT "wl-dock project")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ld/wl-dock")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
set(CPACK_PACKAGE_RELOCATABLE FALSE)

# 包内安装前缀：FHS 标准前缀 /usr
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

# 包文件名：wl-dock-<version>-<tag>.{tar.gz,deb,rpm}
# tag 由 -DWL_DOCK_PACKAGE_TAG 传入（如 debian-13-amd64 / fedora-42-x86_64）
set(CPACK_PACKAGE_FILE_NAME "wl-dock-${CPACK_PACKAGE_VERSION}-${WL_DOCK_PACKAGE_TAG}")

# === DEB 配置 ===
set(CPACK_DEBIAN_PACKAGE_NAME "wl-dock")
set(CPACK_DEBIAN_PACKAGE_SECTION "x11")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
# 用 dpkg-shlibdeps 自动从 ELF 依赖推导运行时依赖
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS TRUE)
# 自动推导失败时的兜底依赖（Debian trixie / Ubuntu 24.04+ 的 t64 命名）
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libqt6core6t64, libqt6gui6t64, libqt6widgets6t64, libqt6dbus6t64, libqt6network6t64, libwayland0t64")
# .deb 文件名遵循 Debian 约定：name_version_arch.deb
# 此处仍用统一 tag 命名以便发布页区分发行版/架构
set(CPACK_DEBIAN_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.deb")
# DEB 包安装后自动执行 update-desktop-database / gtk-update-icon-cache
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/postinst;${CMAKE_CURRENT_SOURCE_DIR}/postrm")

# === RPM 配置 ===
set(CPACK_RPM_PACKAGE_NAME "wl-dock")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_RPM_PACKAGE_GROUP "User Interface/X")
set(CPACK_RPM_PACKAGE_SUMMARY "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
# 用 rpm 自动依赖推导
set(CPACK_RPM_PACKAGE_AUTOREQ 1)
# 兜底依赖
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase-gui, qt6-qtbase-widgets, wayland")
set(CPACK_RPM_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.rpm")
# RPM 安装后钩子：刷新桌面数据库与图标缓存
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/postinst")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/postrm")

include(CPack)
