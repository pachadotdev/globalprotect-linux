# Maintainer: Mauricio Vargas <mavargas11@uc.cl>
pkgname=globalprotect-openconnect-git
pkgver=0.0.1
pkgrel=1
pkgdesc="GlobalProtect VPN client for Linux (unofficial)"
arch=('x86_64')
url="https://github.com/pachadotdev/globalprotect-linux"
license=('GPL3')
depends=('qt6-base' 'qt6-webengine' 'qt6-websockets' 'qtkeychain-qt6' 'openconnect')
makedepends=('git' 'cmake')
provides=('globalprotect-openconnect')
conflicts=('globalprotect-openconnect')
source=("git+https://github.com/pachadotdev/globalprotect-linux.git")
sha256sums=('SKIP')

pkgver() {
    cd "$srcdir/globalprotect-linux"
    # Get version from VERSION file
    cat VERSION | tr -d '\n'
}

build() {
    cd "$srcdir/globalprotect-linux"
    
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS_RELEASE=-s \
        -DQT_DEFAULT_MAJOR_VERSION=6 \
        -DCMAKE_INSTALL_PREFIX=/usr
    
    cmake --build build -j$(nproc)
}

package() {
    cd "$srcdir/globalprotect-linux"
    
    DESTDIR="$pkgdir" cmake --install build
}

post_install() {
    systemctl daemon-reload
    echo "To start the service, run:"
    echo "  sudo systemctl enable --now gpservice"
}

post_upgrade() {
    systemctl daemon-reload
    echo "To restart the service, run:"
    echo "  sudo systemctl restart gpservice"
}

pre_remove() {
    systemctl stop gpservice || true
    systemctl disable gpservice || true
}

post_remove() {
    systemctl daemon-reload
}
