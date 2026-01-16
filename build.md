# Manjaro / Arch Linux

## Building from Source

Development tools and dependencies can be installed with:

```bash
sudo pacman -S --needed base-devel cmake qt6-base qt6-webengine qt6-websockets qtkeychain-qt6 openconnect
```

Build with (from the project root directory):

```bash
cd /path/to/globalprotect-linux  # Make sure you're in the project root

rm -rf build

cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS_RELEASE=-s \
    -DQT_DEFAULT_MAJOR_VERSION=6

MAKEFLAGS=-j$(nproc) cmake --build build
```

Test the build with:

Install with:

```bash
sudo cmake --install build
```

If needed:

```bash
sudo systemctl enable gpservice && sudo systemctl start gpservice
```

Uninstall with:

```bash
sudo systemctl stop gpservice && sudo systemctl disable gpservice

sudo rm -f /usr/bin/gpservice /usr/bin/gpclient /usr/share/dbus-1/system.d/com.qt.GPService.conf /usr/share/dbus-1/system-services/com.qt.GPService.service /usr/lib/systemd/system/gpservice.service /usr/share/metainfo/com.qt.gpclient.metainfo.xml /usr/share/applications/com.qt.gpclient.desktop /usr/share/icons/hicolor/scalable/apps/com.qt.gpclient.svg

sudo systemctl daemon-reload
```

## Ubuntu/Fedora

Run these to create the installers:

```bash
# DEB package
./build-deb.sh

# RPM package
./build-rpm.sh

# Arch package
makepkg -s
```
