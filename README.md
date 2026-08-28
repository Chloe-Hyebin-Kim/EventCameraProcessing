# EventCameraProcessing

프로젝트는 이벤트 입력 및 처리 로직을 담당하는 Core Library, Batch Processing용 Console Application, 실시간 확인을 위한 Qt 기반 Diagnostic Application(Windows / Linux 공용)으로 구성함.
<img width="871" height="605" alt="image" src="https://github.com/user-attachments/assets/f8ddd6cd-e0b1-4ad6-85cc-ce4898637d88" />


</br>
</br>
</br>


## Requirements

### Common

| 항목 | 최소 버전 | 비고 |
|---|---|---|
| CMake | 3.16 (일반 빌드) / **3.21+ 권장** | `CMakePresets.json`(schema version 3)은 3.21 이상 필요.</br> VS2019 16.11 번들 CMake는 약 3.20대라 preset 일부 기능이 불안정할 수 있음(아래 "알려진 이슈" 참고). |
| C++ 표준 | C++17 | `CMAKE_CXX_STANDARD 17` |
| OpenCV | 4.4.0 | Windows는 리포에 번들(`ocv440/` + 루트 `opencv_world440(d).dll`)되어 있어 별도 설치 불필요, CMake가 자동 감지.</br> Linux는 시스템 패키지 사용(아래 참고), 4.x대면 대체로 호환. |
| Qt | Qt5 ≥ 5.14 또는 Qt6 (Widgets 모듈) | `QImage::Format_BGR888` 사용 때문에 5.14 미만은 안 됨. |
| Metavision SDK (Prophesee) | **5.2.0** (리포 `Prophesee/`에 번들된 버전) | Live 카메라 / RAW 재생(`EventProcessing.DiagQt`)에 필수.</br> 없어도 `EventProcessing.Core`/`EventProcessing.Console`은 CSV 입력만으로 빌드됨.</br> 필요 컴포넌트: `base`, `core`, `stream` (+ 내부적으로 `MetavisionHAL`, `MetavisionPSEEHWLayer`, `hdf5_ecf` 사용 — 전부 `Prophesee/`에 같이 번들됨). |
| Boost | `timer` 컴포넌트만 | `Prophesee/`에 번들 안 되어 있음, 별도 설치 필요 (Metavision SDK의 `core` 모듈이 요구).</br> MSVC 툴셋 버전과 맞는 사전빌드 바이너리 권장(빌드 안 해도 됨). |

### Windows

- **Visual Studio**: 2019(16.11+) 또는 2022, **"C++를 사용한 CMake 도구"** 컴포넌트 + "C++를 사용한 데스크톱 개발" 워크로드
- **Qt**: Qt Online Installer로 설치. VS 버전에 맞는 키트 필요
  - VS2019 → `MSVC 2019 64-bit` 키트 (Qt 6.5 LTS까지 제공, 또는 Qt 5.15.2)
  - VS2022 → `MSVC 2022 64-bit` 키트 (Qt 6.6+ 포함 최신)
- **OpenCV**: 별도 설치 불필요 (리포 번들 자동 감지)
- **Metavision SDK**: 별도 설치 불필요 (리포 `Prophesee/` 자동 감지, 시스템 설치본이 있으면 그건 대신 무시하고 리포 번들본을 우선 사용함)
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
- **Metavision SDK (OpenEB)**: 리포에 번들 안 되어 있음(리포 `Prophesee/`는 Windows 바이너리만 포함) — 연구실 Linux 머신들이 배포판/버전이 제각각이라, Windows처럼 미리 빌드된 바이너리 하나로 커밋해둘 수가 없다. 대신 한 번 소스 빌드해서 설치하는 스크립트를 제공한다:
  ```bash
  ./scripts/setup-linux-openeb.sh          # /usr/local에 설치 (sudo 필요)
  # sudo 권한이 없으면:
  ./scripts/setup-linux-openeb.sh --user   # ~/.local/openeb에 설치, CMAKE_PREFIX_PATH 안내가 출력됨
  ```
  리포에 번들된 Windows용과 동일하게 **5.2.0** 버전을 빌드하므로(`Prophesee/include/metavision/sdk/version.h` 참고), RAW/HDF5 파일이 Windows/Linux 양쪽에서 동일하게 열린다. 빌드는 OpenEB 자체가 커서 시간이 좀 걸린다. 완료 후 `cmake --preset linux`를 다시 돌리면(이미 configure된 상태였다면 `build/linux/` 삭제 후) `EventProcessing.DiagQt`까지 같이 빌드된다.
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
       - 번들 Metavision SDK(`Prophesee/`)를 `CMAKE_PREFIX_PATH` 최우선으로 추가
      
       
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
├─ Prophesee/
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
Prophesee/
│
├─ include/
├─ lib/
└─ bin/
```

각 Directory의 역할:

```text
Prophesee/include
        ↓
Compile-time Header

Prophesee/lib
        ↓
Link-time Import Library

Prophesee/bin
        ↓
Runtime DLL
```

### Metavision SDK Path

`Metavision.props`에서 Metavision SDK 경로를 관리함.

Repository 내부에 `Prophesee/`가 존재하면 해당 경로를 우선 사용하고, 존재하지 않을 경우 System-wide Metavision SDK 경로를 사용하도록 구성함.

```text
1. $(SolutionDir)Prophesee

        ↓ if not found

2. C:\Program Files\Prophesee
```

개발 PC마다 동일한 System Path를 구성해야 하는 문제를 줄이고 Repository 기준으로 Build Environment를 재현하기 위한 구조.

### Runtime DLL Deployment

`EventProcessing.Console`(MSBuild), 그리고 CMake로 빌드하는 모든 Windows Target(`EventProcessing.Console`, `EventProcessing.DiagQt`)에 동일한 Post-Build Copy Step 적용(`CMakeLists.txt`의 `eventcore_copy_windows_runtime_deps`).

Build 완료 후

```text
Prophesee\bin\*.dll
```

의 Runtime DLL을 실행 파일 Output Directory로 자동 복사함.

```text
Prophesee\bin
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

Metavision SDK(Prophesee)는 [공식 설치 안내](https://docs.prophesee.ai)를 따라 플랫폼별로 별도 설치함(Windows는 기존처럼 리포의 `Prophesee\` 폴더 또는 시스템 설치를 그대로 사용). `find_package(MetavisionSDK)`로 자동 감지되며,

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

#### Windows 실행 파일 만들어서 팀원에게 전달하기

빌드를 못(안) 하는 팀원에게 실행 파일만 전달하고 싶을 때의 절차. Windows 바이너리는 반드시 Windows 머신에서 빌드해야 함(Linux에서 크로스 컴파일 불가).

**1) 사전 준비 (빌드 머신에 최초 1회)**

- **Visual Studio 2019(16.11+) 또는 2022** — 설치 시 워크로드에 "C++를 사용한 데스크톱 개발" + "C++ CMake 도구" 컴포넌트 포함
- **Qt** — [Qt Online Installer](https://www.qt.io/download-qt-installer)로 설치(예: Qt 6.7, `MSVC 2022 64-bit` 키트). 설치 후 환경 변수 `QT_DIR`을 Qt 설치 경로로 지정(예: `C:\Qt\6.7.0\msvc2022_64`)하고 Visual Studio 재시작
- **Boost** — [사전빌드 바이너리](https://sourceforge.net/projects/boost/files/boost-binaries/) 설치(예: `boost_1_8x_0-msvc-14.3-64.exe`). 기본 경로 `C:\local\boost_1_8x_0\`에 두면 CMake가 자동 인식
- OpenCV/Metavision SDK는 리포에 이미 번들되어 있어서(`ocv440\`, `Prophesee\`) 별도 설치 불필요

**2) 빌드**

옵션 A — Visual Studio GUI: 파일 > 폴더 열기(Open Folder)로 리포 루트를 열면 `CMakePresets.json`의 `windows-qt` 프리셋을 VS가 인식함 → 상단 구성 드롭다운에서 `windows-qt` 선택 → 시작 항목으로 `EventProcessing.DiagQt.exe` 선택 → 빌드.

옵션 B — CLI:

```bat
cmake --preset windows-qt
cmake --build build\windows-qt --config Release
```

**3) 결과물**

`build\windows-qt\EventProcessing.DiagQt\Release\EventProcessing.DiagQt.exe`에 생성됨. `CMakeLists.txt`의 post-build 스텝(`eventcore_copy_windows_runtime_deps`)이 Prophesee 런타임 DLL, OpenCV DLL, HAL 플러그인을 실행 파일 옆에 자동으로 복사해주므로 별도로 라이브러리를 챙길 필요 없음(Linux와 달리 수동 패키징 불필요).

**4) 팀원 배포**

`Release\` 폴더 전체(exe + 자동 복사된 DLL들 + `hal_plugins\`)를 그대로 zip해서 전달하면 됨. Windows는 이 리포에 Prophesee/OpenCV가 이미 번들되어 있어서 Linux 때처럼(`scripts/setup-linux-openeb.sh`) 별도 SDK 소스 빌드 단계가 필요 없음 — 빌드 머신에 Qt/Boost만 준비되어 있으면 바로 결과물이 나옴.

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
