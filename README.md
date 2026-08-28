# EventCameraProcessing

이벤트 카메라(Prophesee EVK4 HD) 입력을 처리하는 프로젝트. 이벤트 입력/처리 로직을 담당하는 **Core Library**, 배치 처리용 **Console 앱**, 실시간 확인용 **Qt 기반 Diagnostic GUI**(Windows/Linux 공용)로 구성됨. RAW/CSV/Live 카메라 입력을 받아 노이즈 필터링, 시간 구간 누적, 공 검출(Ball Detection)까지 수행함.

## Windows에서 실행하는 법

**사전 준비 (최초 1회)**

- Visual Studio 2019(16.11+) 또는 2022 — "C++를 사용한 데스크톱 개발" + "C++ CMake 도구" 워크로드 설치
- Qt — [Qt Online Installer](https://www.qt.io/download-qt-installer)로 설치(예: Qt 6.7, `MSVC 2022 64-bit` 키트), 환경 변수 `QT_DIR`을 설치 경로로 지정(예: `C:\Qt\6.7.0\msvc2022_64`) 후 재부팅/재로그인
- Boost — [사전빌드 바이너리](https://sourceforge.net/projects/boost/files/boost-binaries/) 설치(예: `boost_1_8x_0-msvc-14.3-64.exe`), 기본 경로(`C:\local\boost_1_8x_0\`)에 두면 자동 인식
- OpenCV/Metavision SDK는 리포에 번들되어 있어(`ocv440\`, `Prophesee_window\`) 별도 설치 불필요

**빌드 및 실행**

```bat
cmake --preset windows-qt
cmake --build build\windows-qt --config Release

build\windows-qt\EventProcessing.Console\Release\EventProcessing.Console.exe input.csv output 10000 30
build\windows-qt\EventProcessing.DiagQt\Release\EventProcessing.DiagQt.exe
```

필요한 DLL(Prophesee 런타임, OpenCV, HAL 플러그인)은 빌드 시 실행 파일 옆으로 자동 복사됨.

## Linux에서 실행하는 법

**사전 준비 (최초 1회)**

```bash
sudo apt install cmake build-essential libopencv-dev qt6-base-dev
```

Metavision SDK(Prophesee)는 리포에 번들되어 있지 않아 한 번 소스 빌드해서 설치해야 함(카메라 없이 CSV만 쓸 거면 생략 가능, 그 경우 `EventProcessing.DiagQt`는 빌드에서 제외됨):

```bash
./scripts/setup-linux-openeb.sh          # /usr/local에 설치 (sudo 필요)
# 또는 sudo 없이: ./scripts/setup-linux-openeb.sh --user
```

**빌드 및 실행**

```bash
cmake --preset linux
cmake --build build/linux -j

./build/linux/EventProcessing.Console/EventProcessing.Console input.csv output 10000 30
./build/linux/EventProcessing.DiagQt/EventProcessing.DiagQt   # Metavision SDK가 감지된 경우에만 빌드됨
```
