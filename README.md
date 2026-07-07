# wl-dock

A lightweight Qt panel/taskbar designed specifically for wlroots-based Wayland compositors, using the layer-shell protocol to anchor the dock at the bottom of the screen.

![wl-dock](wl-dock.png)

## Project Overview

`wl-dock` is a Wayland desktop taskbar and panel application. It obtains the current window list through the `wlr-foreign-toplevel-management-v1` protocol and displays task buttons and a clock as an overlay at the bottom of each connected display.

## Key Features

- Multi-monitor support: automatically creates a bottom dock for every screen.
- Window task buttons: dynamically generates task buttons from the toplevel list provided by `wlr-foreign-toplevel-management-v1`.
- Icon resolution: looks up the corresponding `.desktop` file based on the window `app_id` and loads the icon; falls back to a default icon if no match is found.
- Activation switching: clicking a task button activates the corresponding window.
- Auto-hide behavior: the dock is hidden at a 1px height by default, expands when the mouse enters, and retracts after a short delay when the mouse leaves.
- Clock display: shows current time and date on the right side, refreshed aligned to the minute boundary.

## Architecture

- Qt6 Widgets: handles UI, event loop, and window management.
- LayerShellQt: displays the `wl-dock` window as a layer-shell panel on Wayland.
- Wayland protocol: uses `wayland-scanner` to generate client code for `wlr-foreign-toplevel-management-unstable-v1`.
- Multi-window single-process: creates one `DockWindow` per screen while sharing a single `ForeignToplevelManager` instance.

## Code Structure

- `src/main.cpp`: application entry point, initializes Qt and LayerShellQt, and starts `DockController`.
- `src/DockController.*`: manages screens, creates/removes dock windows, and shares the `ForeignToplevelManager` and icon resolver.
- `src/DockWindow.*`: per-screen dock window, implements hide/expand logic and layer-shell configuration.
- `src/TaskManager.*`: manages the task button list and responds to toplevel add/remove/change events.
- `src/TaskButton.*`: custom button component that displays app icons and active state.
- `src/ClockWidget.*`: clock widget that shows current time and date.
- `src/ForeignToplevelManager.*`: independent Wayland connection integrated with `wlr-foreign-toplevel-management-v1`, listens to toplevel events.
- `src/DesktopIconResolver.*`: searches for `.desktop` files and resolves icon names or paths.

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

`wl-dock` depends on a wlroots-compatible Wayland compositor and requires layer-shell protocol support.

## Build and Run

```bash
cmake -B build
cmake --build build
./build/wl-dock
```

If the `build` directory already exists, you can simply run `cmake --build build`.

## Usage

1. Run `./build/wl-dock`.
2. The application automatically detects the current Wayland screens and creates a dock at the bottom of each display.
3. Move the mouse to the bottom of the screen to expand the dock.
4. Click a task button to activate the corresponding window.
5. When the mouse leaves the dock area or you click outside, the dock will automatically retract after a short delay.

## Notes

- Supports Wayland only; it does not work on X11.
- Requires proper screen management and layer-shell support.
- The application does not quit automatically when the last window closes, so it can remain running in the background during TTY switches.

## Contributing and Discussion

Contributions are welcome! If you want to join development, discuss features, or report issues, please:

- open an issue to start a discussion,
- submit a pull request with bug fixes or feature improvements,
- suggest UI and usability enhancements,
- help improve Wayland compatibility and layer-shell behavior.

We appreciate contributions from the community and encourage developers to participate in improving `wl-dock`.

## License

`wl-dock` is licensed under the GNU General Public License v3.0 (GPL-3.0).
