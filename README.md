# wl-dock

A simple dock for wlroots compositor.

![wl-dock](wl-dock.png)

## Build from the source code

### Build dependencies

```
apt install build-essential cmake pkgconf
apt install qt6-base-dev qt6-tools-dev qt6-wayland-dev wayland-protocols liblayershellqtinterface-dev
```

### Runtime dependencies

```
apt install layer-shell-qt liblayershellqtinterface6
```

### Build

```
cmake -B build
cmake --build build
```

### Run it

```
./build/wl-dock
```

## License

Crystal Dock is licensed under the GNU General Public License v3.0
