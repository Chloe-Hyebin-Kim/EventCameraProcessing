#!/usr/bin/env bash
# Prophesee OpenEB(Metavision SDK의 오픈소스 에디션)를 Linux에서 소스로 클론/빌드/설치한다.
#
# 왜 바이너리를 리포에 번들하지 않고 스크립트로 빌드하나:
#   Windows(Prophesee/)는 배포판이 하나(MSVC x64)라 미리 빌드된 바이너리를 그대로 커밋해도 되지만,
#   연구실 Linux 머신들은 배포판/버전이 제각각이라 미리 빌드된 바이너리 하나로는 다 커버할 수 없다.
#   반면 소스 빌드는 (빌드 의존 패키지만 깔려 있으면) 어떤 배포판에서도 동작한다.
#
# 왜 5.2.0인가:
#   Windows에 번들된 Prophesee/의 Metavision SDK도 5.2.0이다(Prophesee/include/metavision/sdk/version.h).
#   버전을 맞춰야 같은 RAW/HDF5 파일을 Windows/Linux 양쪽에서 동일하게 읽고 쓸 수 있다.
#
# 사용법:
#   ./scripts/setup-linux-openeb.sh              # /usr/local에 설치 (sudo 필요, 기본값)
#   ./scripts/setup-linux-openeb.sh --user        # sudo 없이 ~/.local/openeb에 설치
#   ./scripts/setup-linux-openeb.sh --jobs 8      # 병렬 빌드 job 수 (기본: nproc)
#
# 참고(공식 빌드 안내): https://docs.prophesee.ai/stable/installation/linux_openeb.html
# (Ubuntu 22.04/24.04 기준 안내지만, 빌드 의존 패키지가 apt로 깔리는 다른 Debian 계열 배포판에서도
#  대체로 동작한다 - 안 되면 위 문서에서 배포판에 맞는 패키지명을 확인해서 스크립트를 조정할 것.)

set -euo pipefail

OPENEB_VERSION="5.2.0"
SRC_DIR="${OPENEB_SRC_DIR:-$HOME/.cache/openeb-src}"
JOBS="$(nproc 2>/dev/null || echo 4)"
INSTALL_MODE="system"
PREFIX="/usr/local"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --user)
            INSTALL_MODE="user"
            PREFIX="$HOME/.local/openeb"
            shift
            ;;
        --prefix)
            PREFIX="$2"
            INSTALL_MODE="custom"
            shift 2
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [--user] [--prefix DIR] [--jobs N]"
            echo "  --user         install to ~/.local/openeb without sudo (default: /usr/local, needs sudo)"
            echo "  --prefix DIR   install to a custom prefix instead"
            echo "  --jobs N       parallel build jobs (default: nproc)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

echo "==> [1/6] Installing OpenEB build dependencies (apt, needs sudo)..."
sudo apt-get update
sudo apt-get install -y \
    apt-utils build-essential software-properties-common wget unzip curl git cmake \
    libopencv-dev libboost-all-dev libusb-1.0-0-dev libprotobuf-dev protobuf-compiler \
    libhdf5-dev hdf5-tools libglew-dev libglfw3-dev libcanberra-gtk-module ffmpeg

echo "==> [2/6] Fetching OpenEB ${OPENEB_VERSION} source into ${SRC_DIR}..."
if [[ -d "$SRC_DIR/.git" ]]; then
    echo "    (existing checkout found at $SRC_DIR, reusing it - remove that directory to re-clone)"
else
    git clone --branch "$OPENEB_VERSION" --depth 1 https://github.com/prophesee-ai/openeb.git "$SRC_DIR"
fi

BUILD_DIR="$SRC_DIR/build"
echo "==> [3/6] Configuring (Release, tests off) into ${BUILD_DIR}..."
# COMPILE_PYTHON3_BINDINGS=OFF: EventCameraProcessing only uses the C++ SDK (base/core/stream),
# never the Python bindings. Leaving it ON (OpenEB's default) requires pybind11 >= 2.7, which is
# newer than what apt ships on some distros (e.g. pybind11 2.4.3 on Ubuntu 20.04/focal) - that
# version mismatch makes the configure step fail outright. Turning it off sidesteps the pybind11
# requirement entirely, and we don't lose anything we actually need.
cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DCOMPILE_PYTHON3_BINDINGS=OFF \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"

echo "==> [4/6] Building with ${JOBS} parallel jobs (OpenEB is large - this can take a while)..."
cmake --build "$BUILD_DIR" --config Release -- -j "$JOBS"

echo "==> [5/6] Installing to ${PREFIX}..."
if [[ "$INSTALL_MODE" == "system" ]]; then
    sudo cmake --build "$BUILD_DIR" --target install
else
    mkdir -p "$PREFIX"
    cmake --build "$BUILD_DIR" --target install
fi

echo "==> [6/6] Installing udev rules for Prophesee cameras (needs sudo; harmless if you don't have one yet)..."
if compgen -G "$SRC_DIR/hal_psee_plugins/resources/rules/*.rules" > /dev/null; then
    sudo cp "$SRC_DIR"/hal_psee_plugins/resources/rules/*.rules /etc/udev/rules.d/
    sudo udevadm control --reload-rules
    sudo udevadm trigger
else
    echo "    (no udev rules files found in this OpenEB checkout - skipping)"
fi

echo ""
echo "Done - OpenEB ${OPENEB_VERSION} installed to ${PREFIX}."
if [[ "$INSTALL_MODE" != "system" ]]; then
    echo ""
    echo "You installed to a non-system prefix, so CMake won't find it automatically."
    echo "Add this to your shell profile (~/.bashrc or ~/.profile), then open a new shell:"
    echo ""
    echo "  export CMAKE_PREFIX_PATH=\"$PREFIX:\$CMAKE_PREFIX_PATH\""
    echo ""
fi
echo "Now (re)configure EventCameraProcessing - if it was already configured without Metavision SDK,"
echo "delete its build/linux/ directory first so CMake re-runs find_package(MetavisionSDK) from scratch:"
echo ""
echo "  rm -rf build/linux && cmake --preset linux"
echo ""
echo "You should see: 'Metavision SDK found - Live camera / RAW support enabled (EventProcessing.DiagQt included)'"
