# EventCameraProcessing

프로젝트는 이벤트 입력 및 처리 로직을 담당하는 Core Library, Batch Processing용 Console Application, 실시간 확인을 위한 Qt 기반 Diagnostic Application(Windows / Linux 공용)으로 구성함.
<img width="871" height="605" alt="image" src="https://github.com/user-attachments/assets/f8ddd6cd-e0b1-4ad6-85cc-ce4898637d88" />


</br>
</br>
</br>


## Quick Start (소스 받아서 GUI 실행하기)

> **`QtVer` 브랜치 기준.** GUI(`EventProcessing.DiagQt`) 실행에 필요한 인프라(Linux용 OpenEB 빌드 스크립트, Windows Release 강제, DLL 자동 배포 등)가 현재 `QtVer` 브랜치에만 있으므로, 아래 단계는 전부 `QtVer`를 받았다는 전제로 진행함. 더 자세한 배경/트러블슈팅은 아래 [Requirements](#requirements), [알려진 이슈](#알려진-이슈) 섹션 참고.

### Windows

1. **필수 프로그램 설치**
   - Visual Studio 2019(16.11+) 또는 2022 — 설치 시 **"C++를 사용한 데스크톱 개발"** 워크로드 + **"C++ CMake 도구"** 컴포넌트 체크
   - Qt — [Qt Online Installer](https://www.qt.io/download-qt-installer)로 설치. VS2022면 `MSVC 2022 64-bit` 키트(Qt 6.x) 선택
   - Boost — [사전빌드 바이너리](https://sourceforge.net/projects/boost/files/boost-binaries/) 설치 (VS2022 → `boost_1_8x_0-msvc-14.3-64.exe`, 기본 경로 `C:\local\boost_1_8x_0\` 그대로 두면 자동 감지됨)
   - OpenCV / Metavision SDK는 리포에 번들되어 있어 별도 설치 불필요

2. **Qt 경로를 환경 변수로 등록**: 시스템 환경 변수에 `QT_DIR`을 Qt 설치 경로로 지정(예: `C:\Qt\6.7.0\msvc2022_64`). 설정 후 Visual Studio를 껐다 다시 켜야 반영됨.

3. **소스 받기**
   ```bash
   git clone --branch QtVer https://github.com/Chloe-Hyebin-Kim/EventCameraProcessing
   ```

4. **Visual Studio에서 열기**: `EventCameraProcessing` 폴더를 **파일 > 폴더 열기(Open Folder)** 로 연다. (`.sln`을 더블클릭하는 게 아님 — 거기엔 GUI가 안 들어있음.)

5. 상단 구성 드롭다운에서 **`windows-qt`** 를 선택하면 CMake가 자동으로 configure됨(잠시 대기).

6. **빌드**: 상단 메뉴 `빌드 > 모두 빌드`(또는 `Ctrl+Shift+B`).

7. **실행**: 시작 항목을 `EventProcessing.DiagQt.exe`로 선택하고 실행(F5), 또는 빌드된 실행 파일을 직접 실행:
   ```
   build\windows-qt\EventProcessing.DiagQt\EventProcessing.DiagQt.exe
   ```

### Linux (Ubuntu/Debian 기준)

1. **필수 패키지 설치**
   ```bash
   sudo apt install cmake build-essential libopencv-dev qt6-base-dev
   # qt6-base-dev가 안 잡히면 대신: qtbase5-dev
   ```

2. **소스 받기**
   ```bash
   git clone --branch QtVer https://github.com/Chloe-Hyebin-Kim/EventCameraProcessing
   cd EventCameraProcessing
   ```

3. **Metavision SDK(OpenEB) 준비** — 두 가지 방법이 있음.

   **방법 A: 번들 그대로 써보기 (`Prophesee-linux/`, 별도 설치/빌드 없음)**

   리포에 Ubuntu 20.04에서 빌드해 번들해둔 OpenEB + 의존 라이브러리(`Prophesee-linux/`)가 들어있어서, 그냥 clone만 하면 3번 단계 없이 바로 4번(Configure + 빌드)으로 넘어가도 됨. 다만 **그 컴퓨터의 시스템 OpenCV 버전이 번들(4.2)과 크게 다르면(예: Ubuntu 22.04/24.04의 기본 OpenCV 4.5+/4.6+) 빌드 시점에 라이브러리 버전 충돌로 링크가 실패할 수 있음** — 실제로 확인된 증상: `EventProcessing.Console`/`EventProcessing.DiagQt` 링크 단계에서 `libgdal`/`libgeos`/`libspatialite` 등에서 `undefined reference` 에러가 무더기로 남. 이 경우 아래 방법 B로 넘어갈 것.

   **방법 B: 소스에서 직접 빌드/설치 (방법 A가 안 될 때, 또는 처음부터 이쪽을 원하면)**

   그 컴퓨터의 실제 시스템 라이브러리 버전에 맞춰 새로 빌드하므로 방법 A의 버전 충돌 문제가 없음. 대신 OpenEB 자체가 커서 시간이 좀 걸림.
   ```bash
   ./scripts/setup-linux-openeb.sh          # /usr/local에 설치 (sudo 필요)
   # sudo 권한이 없으면:
   ./scripts/setup-linux-openeb.sh --user   # ~/.local/openeb에 설치 (완료 후 안내되는 환경변수 추가 필요)
   ```
   방법 B로 설치하면 CMake가 시스템 설치본을 우선 쓰므로, 리포의 `Prophesee-linux/`는 무시됨(`CMakeLists.txt`가 `Prophesee-linux/`를 시스템보다 먼저 검색 경로에 넣긴 하지만, `find_package`가 성공하는 쪽이 어차피 먼저 잡힌 그 경로이므로 실질적으로 방법 A가 실패하지 않는 한 방법 B 설치본이 쓰일 일은 없음 — 방법 A가 안 되면 `rm -rf Prophesee-linux`로 아예 지우고 방법 B만 쓸 것).

4. **Configure + 빌드**
   ```bash
   cmake --preset linux
   cmake --build build/linux -j$(nproc)
   ```
   configure 로그에 `Metavision SDK found - Live camera / RAW support enabled` 가 보여야 `EventProcessing.DiagQt`까지 같이 빌드됨. 안 보이면 3번을 아직 안 했거나(방법 A만 있고 B는 안 했는데 A도 실패한 경우 등) 새 셸을 안 열었을 가능성이 큼. configure는 통과했는데 **빌드(링크) 단계에서 `undefined reference to ...` 에러**가 나면 방법 A의 버전 충돌이니 3번의 방법 B로 넘어갈 것.

5. **실행**
   ```bash
   ./build/linux/EventProcessing.DiagQt/EventProcessing.DiagQt
   ```
   `error while loading shared libraries: libmetavision_sdk_core.so.5` 같은 에러가 뜨면 `sudo ldconfig` 한 번 실행 후 다시 시도.

## Requirements

### Common

| 항목 | 최소 버전 | 비고 |
|---|---|---|
| CMake | 3.16 (일반 빌드) / **3.21+ 권장** | `CMakePresets.json`(schema version 3)은 3.21 이상 필요.</br> VS2019 16.11 번들 CMake는 약 3.20대라 preset 일부 기능이 불안정할 수 있음(아래 "알려진 이슈" 참고). |
| C++ 표준 | C++17 | `CMAKE_CXX_STANDARD 17` |
| OpenCV | 4.4.0 | Windows는 리포에 번들(`ocv440/` + 루트 `opencv_world440(d).dll`)되어 있어 별도 설치 불필요, CMake가 자동 감지.</br> Linux는 시스템 패키지 사용(아래 참고), 4.x대면 대체로 호환. |
| Qt | Qt5(버전 무관) 또는 Qt6 (Widgets 모듈) | 미리보기 프레임 변환에 `Format_RGB888` + `rgbSwapped()`를 쓰므로(Qt4 때부터 있는 API) 특정 최소 버전 제약 없음. |
| Metavision SDK (Prophesee) | **5.2.0** (리포 `Prophesee-window/`에 번들된 버전) | Live 카메라 / RAW 재생(`EventProcessing.DiagQt`)에 필수.</br> 없어도 `EventProcessing.Core`/`EventProcessing.Console`은 CSV 입력만으로 빌드됨.</br> 필요 컴포넌트: `base`, `core`, `stream` (+ 내부적으로 `MetavisionHAL`, `MetavisionPSEEHWLayer`, `hdf5_ecf` 사용 — 전부 `Prophesee-window/`에 같이 번들됨). |
| Boost | `timer` 컴포넌트만 | `Prophesee-window/`에 번들 안 되어 있음, 별도 설치 필요 (Metavision SDK의 `core` 모듈이 요구).</br> MSVC 툴셋 버전과 맞는 사전빌드 바이너리 권장(빌드 안 해도 됨). |

### Windows

- **Visual Studio**: 2019(16.11+) 또는 2022, **"C++를 사용한 CMake 도구"** 컴포넌트 + "C++를 사용한 데스크톱 개발" 워크로드
- **Qt**: Qt Online Installer로 설치. VS 버전에 맞는 키트 필요
  - VS2019 → `MSVC 2019 64-bit` 키트 (Qt 6.5 LTS까지 제공, 또는 Qt 5.15.2)
  - VS2022 → `MSVC 2022 64-bit` 키트 (Qt 6.6+ 포함 최신)
- **OpenCV**: 별도 설치 불필요 (리포 번들 자동 감지)
- **Metavision SDK**: 별도 설치 불필요 (리포 `Prophesee-window/` 자동 감지, 시스템 설치본이 있으면 그건 대신 무시하고 리포 번들본을 우선 사용함)
- **Boost**: [사전빌드 바이너리](https://sourceforge.net/projects/boost/files/boost-binaries/) 설치
  - VS2019(MSVC 14.2) → `boost_1_8x_0-msvc-14.2-64.exe`
  - VS2022(MSVC 14.3) → `boost_1_8x_0-msvc-14.3-64.exe`
  - 기본 설치 경로(`C:\local\boost_1_8x_0\`) 그대로 두면 CMake가 자동 감지
- **vcpkg**: 필수 아님(OpenCV/Metavision SDK가 리포에 번들되어 있어서). Boost나 Qt를 vcpkg로 관리하고 싶으면 대신 사용 가능(리포에 `vcpkg.json`/`vcpkg-configuration.json` 있지만 현재 실제로 소비되는 패키지는 없음).

### Linux (Ubuntu/Debian 기준)

- **Qt6**
  ```bash
  sudo apt install cmake build-essential libopencv-dev qt6-base-dev
  # Qt6 안 될 경우 대체: qtbase5-dev
  ```
- **Metavision SDK (OpenEB)**: 두 가지 경로가 있음.
  - **번들(`Prophesee-linux/`)**: Ubuntu 20.04에서 빌드한 OpenEB(버전 **5.2.0**, `Prophesee-window/include/metavision/sdk/version.h`에 명시된 Windows 번들판과 동일)와, 그게 링크하는 OpenCV/Boost/HDF5/Protobuf/ffmpeg 등의 `.so`까지 `ldd`로 수집해 같이 커밋해뒀음(`readelf -d` NEEDED 기준 실제 필요한 것만 - GDAL/PostgreSQL/Kerberos까지 딸려오는데, `libopencv_videoio.so`가 실제로 이것들에 링크되어 있어서 어쩔 수 없음). `CMakeLists.txt`가 자동으로 찾아 쓰므로 clone만 하면 별도 설치 없이 바로 빌드됨.
    **단, 그 컴퓨터의 시스템 OpenCV 버전이 번들(4.2)과 많이 다르면(Ubuntu 22.04/24.04 등) 빌드 시점에 `libgdal`/`libgeos`/`libspatialite` 등에서 `undefined reference` 링크 에러가 날 수 있음** — 번들이 링크하는 구버전 GEOS/GDAL과 시스템 OpenCV가 링크하는 신버전 GDAL이 같은 실행 파일 안에서 충돌하는 것. 이럴 땐 아래 스크립트로 넘어갈 것.
  - **소스 빌드 스크립트** (번들이 버전 충돌 날 때, 또는 처음부터 이쪽을 원할 때): 그 컴퓨터의 실제 시스템 라이브러리 버전에 맞춰 새로 빌드하므로 위 버전 충돌이 없음.
    ```bash
    ./scripts/setup-linux-openeb.sh          # /usr/local에 설치 (sudo 필요)
    # sudo 권한이 없으면:
    ./scripts/setup-linux-openeb.sh --user   # ~/.local/openeb에 설치, CMAKE_PREFIX_PATH 안내가 출력됨
    ```
    OpenEB 자체가 커서 빌드에 시간이 좀 걸림. 번들이 버전 충돌로 실패한 상태였다면 `rm -rf Prophesee-linux`로 지운 뒤 이 스크립트를 쓸 것(그래야 `CMakeLists.txt`가 시스템 설치본을 확실히 사용함). 완료 후 `cmake --preset linux`를 다시 돌리면(이미 configure된 상태였다면 `build/linux/` 삭제 후) `EventProcessing.DiagQt`까지 같이 빌드된다.
- **Boost**: `sudo apt install libboost-timer-dev` (Metavision SDK를 쓸 경우에만 필요 - 위 스크립트를 쓰면 `libboost-all-dev`로 이미 같이 설치됨)


### 환경 변수

| 변수 | 필수 여부 | 설명 |
|---|---|---|
| `QT_DIR` | Windows에서 Qt 자동 감지용 (권장) | Qt 키트 경로, 예: `C:\Qt\6.5.3\msvc2019_64`. CMake가 `CMAKE_PREFIX_PATH`에 자동 추가함. |
| `QTDIR` | `QT_DIR` 없을 때 폴백으로 사용됨 | Qt 설치 관례상의 변수명. </br>이미 다른 용도로 설정되어 있을 수 있으니 가능하면 `QT_DIR`을 새로 쓰는 걸 권장. |
| `BOOST_ROOT` | 보통 불필요 | Boost를 기본 경로(`C:\local\boost_*`)가 아닌 곳에 설치했을 때만 필요. |
| `MV_HAL_PLUGIN_PATH` | 설정 불필요 | 앱이 실행 시점에 자동으로 설정함 </br> (`EnsureBundledHalPluginPath()`, 실행 파일 옆 `hal_plugins\` 탐색). 수동 설정 시 그 값이 우선됨. |
| `OpenCV_DIR` | 설정 불필요 | 번들 OpenCV를 CMake가 IMPORTED 타깃으로 직접 구성하므로 불필요 </br>(다른 OpenCV로 덮어쓰고 싶을 때만 지정). |



### CMake 옵션

| 옵션 | 기본값 | 설명 |
|---|---|---|
| `EVENTCORE_NO_METAVISION` | `OFF` | `ON`으로 주면 Metavision SDK가 있어도 강제로 안 씀 </br>(CSV 입력만 지원, `EventProcessing.DiagQt` 제외). |
| `CMAKE_BUILD_TYPE` | `Release`(미지정 시 기본값으로 설정됨) | `Debug`/`Release` |
| `CMAKE_PREFIX_PATH` | - | Qt 등 추가 검색 경로. `windows-qt` CMake preset이 `QT_DIR`로 자동 설정함. |


### 알려진 이슈

- **VS2019에서 CMakePresets의 OS별 자동 필터링이 불안정함**
  - 증상: `windows-qt` preset을 만들어놔도, VS2019(16.11)가 `CMAKE_PREFIX_PATH`가 비어 있는 `linux` preset으로 조용히 configure해버리는 경우가 있었음(`condition` 필드가 기대대로 평가되지 않음).
  - 해결/완화:
    1. VS 상단 툴바의 구성 드롭다운에서 `windows-qt`를 직접 선택
    2. 그와 별개로, `CMakeLists.txt`가 **Windows에서는 어떤 구성이 선택되든** 다음을 자동으로 검색하도록 이미 보강되어 있음:
       - `QT_DIR` → 없으면 `QTDIR` 순으로 Qt 경로 자동 추가
       - 번들 OpenCV(`ocv440/`)를 IMPORTED 타깃으로 직접 구성(별도 옵션 불필요)
       - 번들 Metavision SDK(`Prophesee-window/`)를 `CMAKE_PREFIX_PATH` 최우선으로 추가
      
       
</br>


## Project Structure


```text
EventCameraProcessing/
│
├─ EventProcessing.Core/
│  ├─ Event Source
│  ├─ Event Filtering
│  ├─ Event Accumulation
│  ├─ Ball Detection
│  └─ Ready / Trigger / Capture Logic
│
├─ EventProcessing.Console/
│  └─ RAW / CSV / Live 입력 기반 Batch Processing
│
├─ EventProcessing.DiagQt/
│  └─ Qt Widgets 기반 Live / RAW Diagnostic Viewer (Windows / Linux 공용)
│
├─ Prophesee-window/
│  ├─ include/
│  ├─ lib/
│  └─ bin/
│
├─ ocv440/
│  └─ OpenCV 4.4.0
│
├─ Metavision.props
├─ EventCameraProcessing.sln    (Windows / MSBuild, Core·Console 전용)
├─ CMakeLists.txt               (Windows / Linux 공용, DiagQt(GUI)는 이 빌드로만 생성 가능)
└─ README.md
```

> GUI는 Qt Widgets(`EventProcessing.DiagQt`) 하나로 통일되어 있음. Windows 전용이었던 MFC 버전(`EventProcessing.Diag`)은 제거됨 - 이제 Windows와 Linux가 동일한 GUI 소스와 동일한 `CMakeLists.txt` 빌드를 공유함.
>
> `EventCameraProcessing.sln`(MSBuild)은 `EventProcessing.Core` / `EventProcessing.Console`만 포함하며, Windows에서 기존 Visual Studio 워크플로를 유지하고 싶을 때 계속 사용할 수 있음. GUI(`EventProcessing.DiagQt`)는 Windows에서도 CMake 빌드로만 만들 수 있음 (아래 Build 섹션 참고).

### EventProcessing.Core

이벤트 카메라 데이터 처리에 필요한 핵심 로직을 담당하는 Static Library.

주요 기능:

- CSV Event Stream Loading
- Metavision RAW File Loading
- EVK4 HD Live Camera Input
- Event Data Representation
- Event Noise Filtering
- Time-window Event Accumulation
- Positive / Negative Event Visualization
- Ball Candidate Detection
- Ready / Trigger / Capture State Management

`EventProcessing.Console`, `EventProcessing.DiagQt`에서 공통으로 `EventProcessing.Core`를 사용함.

### EventProcessing.Console

Command-line 기반 Batch Processing Application.

RAW, CSV 또는 Live Camera에서 입력된 Event Stream을 일정 시간 구간으로 분할하고 Event Accumulation Image 및 Debug 결과 생성에 사용함.

주요 출력:

- Positive Event Image
- Negative Event Image
- Merged Event Image
- Binary Mask
- Ball Detection Debug Image
- MP4 Visualization Video

기본 Processing Flow:

```text
RAW / CSV / Live
       │
       ▼
Event Source
       │
       ▼
Noise Filtering
       │
       ▼
Time-window Accumulation
       │
       ▼
Ball Detection
       │
       ▼
Image / Video Output
```

### EventProcessing.DiagQt

Qt Widgets 기반 Diagnostic Application (Windows / Linux 공용).

EVK4 HD Live Camera 또는 RAW Recording의 Event Stream을 실시간으로 확인하고, Ball Detection 결과를 기반으로 Shot Capture 상태를 관리하는 용도로 구성함.

```text
Searching
    │
    ▼
Ball Detection
    │
    ▼
Ready
    │
    ▼
Shot Trigger
    │
    ▼
Capturing
    │
    └──────────► Searching
```

Ball이 일정 시간 동안 동일 위치에서 안정적으로 검출되면 `Ready` 상태로 전환함.

Ready 상태에서 설정한 이동 속도 이상의 변화가 발생하면 Shot으로 판단하고 `Capturing` 상태로 전환함.

---

## Dependencies

### Development Environment

```text
Language        : C++17
IDE             : Visual Studio 2022 (Windows) / 임의 IDE 또는 CLI (Linux)
Platform        : Windows x64, Linux
GUI             : Qt Widgets (EventProcessing.DiagQt, Windows·Linux 공용)
Build           : CMake (Windows·Linux, 전체 프로젝트 / GUI 포함)
                  MSBuild / Visual Studio Solution (Windows, Core·Console만)
```

### OpenCV

Event Visualization 및 기본 Image Processing에 OpenCV 4.4.0 사용.

```text
OpenCV 4.4.0
```

Repository 내부 경로:

```text
ocv440/
```

주요 사용 용도:

- Event Accumulation Image 생성
- Binary Image Processing
- Contour Detection
- Debug Visualization
- PNG 저장
- MP4 Video 생성

### Prophesee Metavision / OpenEB

EVK4 HD / IMX636 Camera 및 Metavision RAW File 처리를 위해 Metavision SDK 5.x API 사용.

주요 Module:

```text
Metavision SDK Base
Metavision SDK Core
Metavision SDK Stream
Metavision HAL
```

SDK Header, Import Library, Runtime DLL을 Repository 내부에서도 참조할 수 있도록 구성함.

```text
Prophesee-window/
│
├─ include/
├─ lib/
└─ bin/
```

각 Directory의 역할:

```text
Prophesee-window/include
        ↓
Compile-time Header

Prophesee-window/lib
        ↓
Link-time Import Library

Prophesee-window/bin
        ↓
Runtime DLL
```

### Metavision SDK Path

`Metavision.props`에서 Metavision SDK 경로를 관리함.

Repository 내부에 `Prophesee-window/`가 존재하면 해당 경로를 우선 사용하고, 존재하지 않을 경우 System-wide Metavision SDK 경로를 사용하도록 구성함.

```text
1. $(SolutionDir)Prophesee-window

        ↓ if not found

2. C:\Program Files\Prophesee
```

개발 PC마다 동일한 System Path를 구성해야 하는 문제를 줄이고 Repository 기준으로 Build Environment를 재현하기 위한 구조.

### Runtime DLL Deployment

`EventProcessing.Console`(MSBuild), 그리고 CMake로 빌드하는 모든 Windows Target(`EventProcessing.Console`, `EventProcessing.DiagQt`)에 동일한 Post-Build Copy Step 적용(`CMakeLists.txt`의 `eventcore_copy_windows_runtime_deps`).

Build 완료 후

```text
Prophesee-window\bin\*.dll
```

의 Runtime DLL을 실행 파일 Output Directory로 자동 복사함.

```text
Prophesee-window\bin
      │
      │ Post-Build
      ▼
x64\Debug
or
x64\Release
```

System-wide `PATH` 설정에 대한 의존성을 줄이기 위한 구성.

### HAL Plugin

EVK4 HD Camera 사용을 위해 일반 Runtime DLL 외에 Metavision HAL Camera Plugin 사용.

HAL Plugin 검색에는 다음 환경변수 사용.

```text
MV_HAL_PLUGIN_PATH
```

프로그램 시작 시 실행 파일 위치를 기준으로 Bundled HAL Plugin Directory를 검색하고 `MV_HAL_PLUGIN_PATH`를 설정하도록 구성함.

```text
EventProcessing.DiagQt.exe
│
├─ Metavision Runtime DLLs
│
└─ hal_plugins/
   ├─ hal_plugin_prophesee.dll
   ├─ metavision_psee_hw_layer.dll
   └─ libusb-1.0.dll
```

별도의 System-wide HAL Plugin Path 설정 없이 실행할 수 있도록 구성함.

### CMake Build (Windows / Linux, GUI 포함)

`CMakeLists.txt`를 이용해 `EventProcessing.Core` / `EventProcessing.Console` / `EventProcessing.DiagQt`를 Windows와 Linux에서 동일한 방식으로 빌드함. GUI(`EventProcessing.DiagQt`)가 필요하면 두 플랫폼 모두 이 빌드를 사용해야 함(`EventCameraProcessing.sln`에는 GUI가 포함되어 있지 않음).

```text
Language        : C++17
Build           : CMake
GUI             : Qt Widgets (Qt6, Qt5로 fallback)
Platform        : Windows, Linux (Ubuntu 기준, 다른 배포판도 가능)
```

필요 패키지 설치:

```bash
# Ubuntu/Debian
sudo apt install cmake build-essential libopencv-dev qt6-base-dev
```

```text
# Windows
- CMake, Visual Studio 2022(C++ 워크로드)
- Qt Online Installer로 Qt 설치 (예: Qt 6.x, MSVC 2022 64bit 키트)
- OpenCV: 리포에 번들된 ocv440\ + 루트의 opencv_world440(d).dll을 CMakeLists.txt가
          자동으로 찾아 쓰므로 별도 설치/옵션 지정 불필요 (원하면 다른 OpenCV로 덮어쓰기 가능)
```

Metavision SDK(Prophesee)는 [공식 설치 안내](https://docs.prophesee.ai)를 따라 플랫폼별로 별도 설치함(Windows는 기존처럼 리포의 `Prophesee-window\` 폴더 또는 시스템 설치를 그대로 사용). `find_package(MetavisionSDK)`로 자동 감지되며,

> **Windows에서 Boost 필요**: Metavision SDK의 `core` 모듈 CMake 설정(`MetavisionSDK_coreConfig.cmake`)이 Boost `timer` 컴포넌트를 요구함(리포에 번들되어 있지 않음). Live 카메라를 안 쓰고 RAW 파일만 보더라도 RAW 디코딩 자체가 Metavision SDK를 거치므로 Boost가 필요함. [Boost 사전빌드 바이너리](https://sourceforge.net/projects/boost/files/boost-binaries/)를 설치(예: `boost_1_8x_0-msvc-14.2-64.exe`, 기본 경로 `C:\local\boost_1_8x_0\`에 설치하면 CMake가 자동으로 찾음, 안 잡히면 `BOOST_ROOT` 환경 변수로 지정).

```text
설치되어 있으면
    → Live 카메라 / RAW 실시간 재생 지원 활성화
    → EventProcessing.DiagQt 포함 전체 빌드

설치되어 있지 않으면
    → CSV 입력만 지원
    → EventProcessing.Core / EventProcessing.Console만 빌드
      (EventProcessing.DiagQt는 실시간 Live/RAW 재생이 목적이라 제외됨)
```

빌드 (Linux):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

빌드 (Windows):

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH="C:\Qt\6.x\msvc2022_64"
cmake --build build --config Release
```

실행:

```bash
./build/EventProcessing.Console/EventProcessing.Console input.csv output 10000 30
./build/EventProcessing.DiagQt/EventProcessing.DiagQt   # Metavision SDK가 감지된 경우에만 빌드됨
```

#### Visual Studio에서 EventProcessing.DiagQt 열기

`EventProcessing.DiagQt`는 CMake 프로젝트라 `EventCameraProcessing.sln`(MSBuild)에는 없음. `.sln`을 더블클릭하는 대신, Visual Studio에서 **파일 > 폴더 열기(Open Folder)** 로 리포 루트 폴더 자체를 열면 VS가 `CMakeLists.txt`/`CMakePresets.json`을 자동 인식해서 `Core`/`Console`/`DiagQt`를 모두 Solution Explorer에 CMake Target으로 보여줌(빌드/디버그 시작 항목으로 `EventProcessing.DiagQt.exe` 선택 가능).

리포에 포함된 `CMakePresets.json`의 `windows-qt` 프리셋을 쓰려면, Windows 환경 변수에 `QT_DIR`을 Qt 설치 경로로 지정(예: `C:\Qt\6.7.0\msvc2022_64`)하고 Visual Studio를 재시작함. OpenCV는 리포의 `ocv440\`을 자동으로 사용하도록 프리셋에 이미 설정되어 있음.

Visual Studio C++용 CMake Tools 컴포넌트(Visual Studio Installer에서 "C++를 사용한 Linux 및 임베디드 개발" 또는 "C++ CMake 도구" 워크로드)가 설치되어 있어야 함. `ninja`도 함께 설치됨.

### Debug / Release

Metavision/OpenEB Library의 Debug / Release Binary를 분리하여 사용함.

```text
Debug
├─ *_d.lib
├─ *_d.dll
└─ /MDd

Release
├─ *.lib
├─ *.dll
└─ /MD
```

Debug와 Release Binary 혼용 시 CRT/STL Symbol 충돌 및 Runtime Dependency 문제가 발생할 수 있으므로 Configuration별로 분리하여 관리함.

---

## Research Direction

본 프로젝트의 현재 목적은 최종 Spin Estimation Algorithm을 바로 구현하는 것이 아니라, 실제 Event Camera Data를 취득·확인하고 후속 연구에 사용할 수 있는 Event Processing Pipeline을 구축하는 것임.

전체 연구 방향:

```text
Event Acquisition
       │
       ▼
Data Validation
       │
       ▼
Event Visualization
       │
       ▼
Noise Filtering
       │
       ▼
Ball ROI Detection
       │
       ▼
Ball Tracking
       │
       ▼
Event-based Optical Flow
       │
       ▼
Dimple / Surface Feature Tracking
       │
       ▼
Rotation Estimation
       │
       ▼
Spin Estimation
```

### Event-based Optical Flow

Event Accumulation Image만 사용하는 방식에서 확장하여 각 Event의 Microsecond-level Timestamp를 이용한 Motion Estimation 적용 예정.

```text
Event = (x, y, t, polarity)
```

Ball Surface에서 발생하는 Event의 시간적 이동 관계를 이용한 Optical Flow 추정 검토.

### Ball Tracking

현재 Contour 기반 Ball Candidate Detection에서 시간적으로 연속된 Ball Position을 추적하는 Tracking 방식으로 확장 예정.

Tracking을 통해 다음 객체와 Ball Event Cluster를 구분하는 방향으로 개선 예정.

```text
Club
Shoe
Player
Background Noise
Ball
```

### Dimple / Surface Feature Tracking

골프공 표면의 Dimple 및 Texture에 의해 발생하는 Event Pattern을 이용한 Surface Motion 추적 검토.

고속 회전 상황에서 Event Camera의 높은 시간 해상도를 활용하여 Frame Camera 기반 방식에서 발생할 수 있는 Motion Blur 및 Temporal Aliasing 문제 감소 가능성 분석 예정.

### Rotation / Spin Estimation

Ball Surface Motion 및 Event-based Optical Flow를 이용하여 Rotation Axis와 Angular Velocity를 추정하는 방향으로 확장 예정.

검토 대상:

```text
Event-based Optical Flow
Temporal Feature Tracking
Ball Geometry Constraint
Camera Calibration
Rotation Model
Physics-based Constraint
```

Event Accumulation Image는 Visualization 및 Debugging 용도로 활용하고, 최종 Motion / Spin Estimation에서는 원본 Event Timestamp 정보를 최대한 유지하는 방향으로 진행 예정.

```text
(x, y, t, polarity)
```

---

## Current Status

### Implemented

- [x] Event Data Structure
- [x] CSV Event Stream Input
- [x] Metavision RAW File Input
- [x] EVK4 HD Live Camera Input
- [x] Event Time-window Accumulation
- [x] Positive Event Visualization
- [x] Negative Event Visualization
- [x] Merged Event Visualization
- [x] Basic Noise Filtering
- [x] Binary Mask Generation
- [x] Basic Ball Candidate Detection
- [x] Debug Image Generation
- [x] PNG Output
- [x] MP4 Visualization
- [x] Console Batch Processing
- [x] Qt Diagnostic Viewer (Windows / Linux)
- [x] CMake Build (Windows / Linux)
- [x] Searching / Ready / Trigger / Capturing State Machine
- [x] Repository-local Metavision SDK Path 구성
- [x] Metavision Runtime DLL Post-Build Copy
- [x] HAL Plugin Path 자동 설정
- [x] Debug / Release Metavision Build 구성

### In Progress / Next

- [ ] Real-world Event Data Quality Evaluation
- [ ] Camera Bias Setting Validation
- [ ] Event Noise Filtering 개선
- [ ] Ball Detection Robustness 개선
- [ ] Temporal Ball Tracking
- [ ] Ball ROI Stabilization
- [ ] Event-based Optical Flow
- [ ] Motion Compensation
- [ ] Dimple / Surface Feature Tracking
- [ ] Rotation Axis Estimation
- [ ] Angular Velocity Estimation
- [ ] Golf Ball Spin Estimation
- [ ] Camera Calibration Integration
- [ ] Physics-based Motion Constraint Integration

현재 개발 단계:

```text
Acquisition         [Done]
        ↓
RAW / Live Loading  [Done]
        ↓
Visualization       [Done]
        ↓
Basic Detection     [Done]
        ↓
Robust Tracking     [Next]
        ↓
Optical Flow        [Next]
        ↓
Rotation / Spin     [Future]
```

---

## Current Limitations

### Ball Detection Robustness

현재 `BallDetector`는 Event Accumulation Image에서 Contour를 분석하여 Ball Candidate를 검출하는 기본 방식 사용.

실제 Golf Swing Event Data에서는 Ball 이외에도 다음 영역에서 많은 Event가 발생함.

```text
Club
Shoe
Player
Background Edge
Lighting Noise
```

가장 큰 Contour 또는 단순 Shape 조건만 사용하는 경우 다른 객체가 Ball로 검출될 가능성이 존재함.

따라서 현재 Ball Detection은 최종 Ball Tracking Algorithm이 아닌 **Data Validation 및 Trigger Logic 검증용 Prototype**에 해당함.

향후 Temporal Tracking 및 ROI 기반 검출 방식 추가 필요.

### Event Noise

Event Camera는 픽셀의 밝기 변화에 반응하므로 Sensor Bias, Lighting, Reflection 및 Background Activity에 따라 Noise Event가 발생할 수 있음.

현재 Basic Noise Filtering 적용 단계.

추후 검토 대상:

```text
Background Activity Filtering
Temporal Filtering
Spatial Filtering
Hot Pixel Removal
ROI Filtering
Motion-based Filtering
```

### Event Accumulation Window

Event Visualization 시 일정 시간 동안 발생한 Event를 하나의 이미지로 누적함.

주요 Parameter:

```text
windowUs
```

Window가 너무 작은 경우:

```text
Event 수 감소
→ Ball Shape 표현 부족
```

Window가 너무 큰 경우:

```text
빠른 Motion이 동일 Frame에 누적
→ Ball Event 확산
→ Motion 정보 중첩
```

Camera Setting, Ball Speed 및 분석 목적에 따른 적절한 Window Size 선정 필요.

### Camera Bias Dependency

Event 발생 특성은 Sensor Bias Setting의 영향을 받음.

Bias가 적절하지 않은 경우:

```text
Background Noise 증가
Event Rate 과다
Ball Surface Event 부족
Feature Contrast 저하
```

등의 문제가 발생할 수 있음.

실제 촬영 환경에서 다음 요소에 대한 반복 실험 필요.

```text
Lighting
Sensor Bias
Camera Position
Ball Distance
Lens
Event Rate
Background
```

### Accumulation Image Information Loss

Event Accumulation Image는 비동기 Event Stream을 사람이 확인하기 쉬운 Image 형태로 표현하는 방법임.

다만 여러 Timestamp의 Event를 하나의 Frame으로 합치므로 원본 Event의 Temporal Information 일부가 손실됨.

```text
Raw Event
(x, y, t, polarity)

        ↓ Accumulation

Image
(x, y, intensity)
```

따라서 후속 Optical Flow / Spin Estimation에서는 Accumulation Image만 사용하는 방식보다 원본 Event Timestamp를 직접 활용하는 처리 방식 병행 필요.

### Spin Estimation

현재 최종 Golf Ball Spin Estimation Algorithm은 구현되지 않은 상태.

현재 구현 범위:

```text
Event Acquisition
        ↓
Visualization
        ↓
Basic Filtering
        ↓
Ball Detection
        ↓
Capture
```

후속 연구 범위:

```text
Ball Tracking
        ↓
Event-based Optical Flow
        ↓
Surface Feature Tracking
        ↓
Rotation Estimation
        ↓
Spin Estimation
```

현재 프로젝트는 위 후속 연구를 진행하기 위한 Event Acquisition / Processing / Visualization 기반 구축 단계임.
