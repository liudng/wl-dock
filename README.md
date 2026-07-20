# wl-dock

A lightweight Qt panel/taskbar designed specifically for wlroots-based Wayland compositors, using the layer-shell protocol to anchor the dock at the bottom of the screen.

![wl-dock](wl-dock.png)

## Project Overview

`wl-dock` is a Wayland desktop taskbar and panel application. It obtains the current window list through the `wlr-foreign-toplevel-management-v1` protocol and displays task buttons and a clock as an overlay at the bottom of each connected display. It also acts as a StatusNotifierItem (SNI) host, showing system tray icons from applications such as Fcitx5, NetworkManager, and Blueman.

## Key Features

- Multi-monitor support: automatically creates a bottom dock for every screen.
- Window task buttons: dynamically generates task buttons from the toplevel list provided by `wlr-foreign-toplevel-management-v1`.
- Icon resolution: looks up the corresponding `.desktop` file based on the window `app_id` and loads the icon; supports Flatpak applications and multi-step fallback matching (X-Flatpak, Keywords, Name, Icon fields).
- Child-window icon fallback: when a child window (e.g. a WeChat chat window) has no `app_id` of its own, falls back to the parent's `app_id`, and finally to the window title.
- Activation switching: clicking a task button activates the corresponding window.
- Auto-hide behavior: the dock is hidden at a 1px height by default, expands when the mouse enters, and retracts after a short delay when the mouse leaves.
- Clock display: shows current time and date on the right side, refreshed aligned to the minute boundary.
- System tray (SNI host): registers `org.kde.StatusNotifierWatcher` on the session bus, hosts tray icons and renders context menus via the `com.canonical.dbusmenu` protocol.
- Custom tooltip: dock-embedded `DockTip` widget instead of `QToolTip`, avoiding layer-shell popup positioning issues.
- Labwc compatibility: custom `SniMenu` widget with `Qt::Window` flag and `AnchorTop|AnchorLeft` anchors, since `QMenu` popups cannot attach to layer-shell surfaces.

## Architecture

- Qt6 Widgets: handles UI, event loop, and window management.
- Qt6 DBus: talks to SNI tray applications and the `com.canonical.dbusmenu` protocol.
- LayerShellQt: displays the `wl-dock` window as a layer-shell panel on Wayland.
- Wayland protocol: uses `wayland-scanner` to generate client code for `wlr-foreign-toplevel-management-unstable-v1`.
- Multi-window single-process: creates one `DockWindow` per screen while sharing a single `ForeignToplevelManager` and `SniWatcher` instance.

## Code Structure

- `src/main.cpp`: application entry point, parses command-line options, initializes Qt and LayerShellQt, and starts `DockController`.
- `src/DockController.*`: manages screens, creates/removes dock windows, and shares the `ForeignToplevelManager`, `DesktopIconResolver`, and `SniWatcher`.
- `src/DockWindow.*`: per-screen dock window, implements hide/expand logic and layer-shell configuration.
- `src/TaskManager.*`: manages the task button list and responds to toplevel add/remove/change events.
- `src/TaskButton.*`: custom button component that displays app icons and active state.
- `src/ClockWidget.*`: clock widget that shows current time and date.
- `src/DockTip.*`: custom tooltip widget rendered as a child of the dock (replaces `QToolTip` for layer-shell compatibility).
- `src/ForeignToplevelManager.*`: independent Wayland connection integrated with `wlr-foreign-toplevel-management-v1`, listens to toplevel events.
- `src/DesktopIconResolver.*`: searches for `.desktop` files and resolves icon names or paths, with Flatpak-aware fallback search.
- `src/sni/SniWatcher.*`: registers the SNI host service on the session bus and tracks `SniItem` lifecycles. UI signals are isolated in `SniWatcherSignals` to prevent QtDBus type-registration warnings.
- `src/sni/SniItem.*`: per-tray-item D-Bus proxy; reads properties, subscribes to `NewIcon` / `NewTitle` / `NewStatus` / `NewToolTip` signals, and decodes SNI pixmap arrays.
- `src/sni/SniTrayWidget.*`: tray icon container, manages `SniIconButton` instances and hides Passive-status items.
- `src/sni/SniIconButton.*`: tray icon button, emits `contextMenuRequested` on right-click.
- `src/sni/SniDbusMenu.*`: `com.canonical.dbusmenu` client; pulls the menu tree via `GetLayout` and renders it with `SniMenu`.
- `src/sni/SniMenu.*`: custom menu widget using `Qt::Window` flag and LayerShellQt margins for Labwc-compatible positioning.
- `src/sni/SniTypes.h`: shared SNI data types (`TrayItemInfo`) and metatype declarations.

## Build Requirements

### Build dependencies

```bash
sudo apt install build-essential cmake pkgconf \
  qt6-base-dev qt6-tools-dev qt6-wayland-dev \
  wayland-protocols liblayershellqtinterface-dev
```

### Runtime dependencies

```bash
sudo apt install layer-shell-qt liblayershellqtinterface6
```

`wl-dock` depends on a wlroots-compatible Wayland compositor (such as Labwc, sway, or Hyprland) and requires layer-shell protocol support. A running D-Bus session bus is required for the system tray to function.

## Build and Run

```bash
cmake -B build
cmake --build build
./build/wl-dock
```

If the `build` directory already exists, you can simply run `cmake --build build`.

## Command-line Options

```
Usage: wl-dock [options]

Options:
  -v, --version              Show version and exit
  -h, --help                 Show this help and exit
      --icon-theme <name>    Set icon theme name (e.g. Adwaita, Breeze, Papirus)
      --default-icon <name>  Set fallback app icon name when .desktop lookup fails
                             (default: application-x-executable)
```

### Examples

```bash
# Use the Papirus icon theme
./build/wl-dock --icon-theme Papirus

# Override the fallback icon used when no .desktop file is found
./build/wl-dock --default-icon applications-development

# Combine both options
./build/wl-dock --icon-theme Breeze --default-icon preferences-desktop-launch-feedback
```

## Usage

1. Run `./build/wl-dock` (optionally with `--icon-theme` and/or `--default-icon`).
2. The application automatically detects the current Wayland screens and creates a dock at the bottom of each display.
3. Move the mouse to the bottom of the screen to expand the dock.
4. Click a task button to activate the corresponding window.
5. Tray icons on the right side reflect running SNI-aware applications. Left-click triggers `Activate`, right-click opens the application's context menu.
6. When the mouse leaves the dock area or you click outside, the dock will automatically retract after a short delay.

## Notes

- Supports Wayland only; it does not work on X11.
- Requires proper screen management and layer-shell support.
- The application does not quit automatically when the last window closes, so it can remain running in the background during TTY switches.
- Tray right-click menus use a custom `SniMenu` widget (not `QMenu`) because `QMenu` popups cannot attach to layer-shell surfaces on Labwc.
- Icon resolution caches results in `DesktopIconResolver::m_cache` to avoid repeated file I/O for the same `app_id`.

## Logging Categories

`wl-dock` uses Qt's categorized logging. Enable a category by setting `QT_LOGGING_RULES`:

```bash
# Enable all wl-dock debug logs
QT_LOGGING_RULES="dock.*=true" ./build/wl-dock

# Enable specific categories
QT_LOGGING_RULES="dock.ftm=true;dock.icon=true" ./build/wl-dock
QT_LOGGING_RULES="dock.sni.menu.popup=true" ./build/wl-dock
```

Available categories include `dock.ctrl`, `dock.ftm` (foreign-toplevel), `dock.icon`, and `dock.sni.*` (tray subsystems).

## Contributing and Discussion

Contributions are welcome! If you want to join development, discuss features, or report issues, please:

- open an issue to start a discussion,
- submit a pull request with bug fixes or feature improvements,
- suggest UI and usability enhancements,
- help improve Wayland compatibility and layer-shell behavior.

We appreciate contributions from the community and encourage developers to participate in improving `wl-dock`.

## License

`wl-dock` is licensed under the GNU General Public License v3.0 (GPL-3.0).
