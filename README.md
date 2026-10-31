# EventCameraProcessing

Prophesee EVK4 HD(IMX636) 이벤트 카메라 데이터를 취득·시각화·처리하기 위한 C++ 프로젝트. 다음 세 부분으로 구성됨.

- **EventProcessing.Core**: 이벤트 입력과 처리 로직을 담은 Static Library. Ball 검출, Shot Trigger, Camera Calibration을 포함함.
- **EventProcessing.Console**: Batch Processing용 Command-line Application.
- **EventProcessing.DiagQt**: 실시간 확인용 Qt 기반 Diagnostic Viewer(Windows / Linux 공용).

<img width="871" height="605" alt="image" src="https://github.com/user-attachments/assets/f8ddd6cd-e0b1-4ad6-85cc-ce4898637d88" />

</br>

## 목차

- [Quick Start](#quick-start)
- [Requirements](#requirements)
- [Project Structure](#project-structure)
- [EventProcessing.Core](#eventprocessingcore)
- [EventProcessing.Console](#eventprocessingconsole)
- [EventProcessing.DiagQt](#eventprocessingdiagqt)
- [Camera Calibration](#camera-calibration)
- [Build 구성 상세](#build-구성-상세)
- [Research Direction](#research-direction)
- [Current Status](#current-status)
- [Current Limitations](#current-limitations)

---

## Quick Start

> 아래 단계는 모두 **`QtVer` 브랜치** 기준임. GUI 빌드에 필요한 구성은 이 브랜치에 있음: 번들 SDK 자동 감지, Windows Release 강제, 런타임 DLL 자동 배포, Linux OpenEB 스크립트 등.

### Windows

1. **필수 프로그램 설치**
   - **Visual Studio 2019(16.11+) 또는 2022**: 설치할 때 **"C++를 사용한 데스크톱 개발"** 워크로드와 **"C++ CMake 도구"** 컴포넌트를 체크.
   - **Qt**: [Qt Online Installer](https://www.qt.io/download-qt-installer)로 VS 버전에 맞는 키트를 설치.
     - VS2019 → `MSVC 2019 64-bit`(Qt 6.5 LTS까지, 또는 Qt 5.15.2)
     - VS2022 → `MSVC 2022 64-bit`
   - OpenCV, Metavision SDK, Boost는 **설치 불필요**(리포 번들 또는 자동 대체, [Requirements](#requirements) 참고).

2. **소스 받기**
   ```bash
   git clone --branch QtVer https://github.com/Chloe-Hyebin-Kim/EventCameraProcessing
   ```

3. **Visual Studio에서 열기**: **파일 > 폴더 열기(Open Folder)** 로 `EventCameraProcessing` 폴더를 연다. `.sln`을 여는 게 아님(`.sln`에는 GUI가 없음).

4. 상단 구성 드롭다운에서 **`windows-qt`** 를 선택하면 CMake configure가 자동으로 시작됨.
   - Qt를 기본 경로(`C:\Qt\<버전>\msvc*_64`)에 설치했다면 자동으로 감지됨.
   - 다른 곳에 설치했다면 시스템 환경 변수 `QT_DIR`에 그 경로를 지정하고 Visual Studio를 재시작.

5. **빌드**: `빌드 > 모두 빌드`(`Ctrl+Shift+B`).

6. **실행**: 시작 항목을 `EventProcessing.DiagQt.exe`로 선택하고 F5를 누른다. 또는 아래 파일을 직접 실행.
   ```
   build\windows-qt\EventProcessing.DiagQt\EventProcessing.DiagQt.exe
   ```

7. **(Live 카메라를 쓸 때만) 카메라 USB 드라이버(WinUSB) 설치**: PC마다 한 번만 하면 됨. RAW 파일 재생만 할 거면 생략 가능.

   앱은 번들된 `libusb-1.0.dll`로 카메라와 통신함(빌드 시 실행 파일 옆 `hal_plugins\`로 자동 복사됨). Windows에서 libusb가 장치에 접근하려면 그 USB 장치에 **WinUSB 드라이버**가 연결되어 있어야 함. 이 드라이버는 리포에 번들할 수 없어서 새 PC마다 직접 설치해야 하고, 설치하지 않으면 Live 모드에서 카메라를 찾지 못함.
   1. 카메라를 USB로 연결.
   2. [Zadig](https://zadig.akeo.ie/)를 다운로드해서 실행(설치 없이 바로 실행됨, 관리자 권한을 요청하면 허용).
   3. 메뉴 `Options > List All Devices` 체크.
   4. 드롭다운에서 Prophesee 카메라를 선택(예: `EVK4`, `Prophesee ...` 등 카메라 모델 이름으로 표시됨). 키보드, 마우스 등 다른 장치를 고르지 않도록 주의. 다른 장치의 드라이버를 바꾸면 그 장치가 동작하지 않게 됨.
   5. 오른쪽(초록 화살표 오른쪽)의 드라이버를 **`WinUSB`** 로 두고 `Install Driver` 클릭. 이미 다른 드라이버가 잡혀 있으면 `Replace Driver` 클릭.
   6. 카메라를 뺐다 다시 꽂고, 앱의 Live 모드에서 카메라가 잡히는지 확인.

   > 정상 설치됐다면 장치 관리자의 `범용 직렬 버스 장치`(Universal Serial Bus devices) 아래에 카메라가 노란 느낌표 없이 보임. 잘못된 장치에 설치했다면 장치 관리자에서 그 장치 우클릭 > `디바이스 제거`(드라이버 삭제 체크) 후 다시 연결하면 원래 드라이버로 돌아감.

### Linux (Ubuntu/Debian 기준)

1. **필수 패키지 설치**
   ```bash
   sudo apt install cmake build-essential libopencv-dev qt6-base-dev
   ```
   `qt6-base-dev`가 없는 배포판이면 대신 `qtbase5-dev`를 설치.

2. **소스 받기**
   ```bash
   git clone --branch QtVer https://github.com/Chloe-Hyebin-Kim/EventCameraProcessing
   cd EventCameraProcessing
   ```

3. **Metavision SDK(OpenEB) 준비**: 둘 중 하나를 고른다.
   - **방법 A: 번들 사용(`Prophesee-linux/`, 추가 작업 없음)**. 리포에 Ubuntu 20.04에서 빌드한 OpenEB 5.2.0과 그 의존 `.so`가 들어 있어서 바로 4번으로 넘어가면 됨.
     단, 시스템 OpenCV 버전이 번들(4.2)과 많이 다르면(Ubuntu 22.04/24.04 등) 링크 단계에서 `libgdal`/`libgeos`/`libspatialite` 등의 `undefined reference` 에러가 날 수 있음. 그러면 방법 B를 사용.
   - **방법 B: 소스에서 빌드/설치**. 그 PC의 라이브러리 버전에 맞춰 빌드하므로 버전 충돌이 없음(빌드 시간이 좀 걸림). 방법 A에서 실패했다면 먼저 `rm -rf Prophesee-linux`를 실행해야 시스템 설치본이 확실히 쓰임.
     ```bash
     ./scripts/setup-linux-openeb.sh          # /usr/local에 설치 (sudo 필요)
     ./scripts/setup-linux-openeb.sh --user   # sudo가 없으면 ~/.local/openeb에 설치 (완료 후 안내되는 환경변수 추가)
     ```

4. **Configure + 빌드**
   ```bash
   cmake --preset linux
   cmake --build build/linux -j$(nproc)
   ```
   configure 로그에 `Metavision SDK found - Live camera / RAW support enabled`가 보여야 `EventProcessing.DiagQt`까지 빌드됨.

5. **실행**
   ```bash
   ./build/linux/EventProcessing.DiagQt/EventProcessing.DiagQt
   ```
   `error while loading shared libraries: libmetavision_sdk_core.so.5` 에러가 뜨면 `sudo ldconfig`를 한 번 실행한 뒤 다시 시도.

6. **(Live 카메라를 쓸 때만) udev 규칙 설치**: 일반 사용자 권한으로 카메라 USB 장치에 접근하기 위해 필요함. PC마다 한 번만 하면 됨. 방법 B 스크립트는 이 단계를 자동으로 수행함.
   ```bash
   sudo cp Prophesee-linux/udev-rules/*.rules /etc/udev/rules.d/ && sudo udevadm control --reload-rules && sudo udevadm trigger
   ```
   설치 후 카메라를 다시 연결.

---

## Requirements

| 항목 | 버전 | 비고 |
|---|---|---|
| CMake | 3.16+ (`CMakePresets.json`은 3.21+) | VS2019 16.11에 들어 있는 CMake는 3.20이라 preset 동작이 불안정할 수 있음([알려진 이슈](#알려진-이슈)). |
| C++ | C++17 | |
| Qt | Qt6 또는 Qt5 (Widgets) | Qt6을 먼저 찾고, 없으면 Qt5를 사용. 특정 최소 버전 제약 없음. |
| OpenCV | 4.x (**calib3d 모듈 필수**) | **Windows**: 리포 번들(`ocv440/` + 루트 `opencv_world440(d).dll`)을 자동으로 사용함.</br>**Linux**: 시스템 `libopencv-dev` 사용. calib3d가 없으면 configure 단계에서 바로 실패함. |
| Metavision SDK | 5.2.0 (`base`, `core`, `stream`) | **Windows**: 리포 번들 `Prophesee-window/`. **Linux**: 번들 `Prophesee-linux/` 또는 스크립트 설치본.</br>SDK가 없으면 CSV 입력만 지원하고 `EventProcessing.DiagQt`는 빌드되지 않음. |
| Boost | 설치 불필요 (Windows) | 번들 SDK의 CMake 설정이 Boost `timer`를 요구하지만, 번들 DLL과 이 프로젝트가 쓰는 헤더는 Boost를 실제로 쓰지 않음. 그래서 Boost가 없으면 `cmake/boost-stub/`의 빈 대체 패키지가 자동으로 쓰임. 실제 Boost가 설치되어 있으면 그쪽을 우선 사용(`C:\local\boost_*` 자동 감지 또는 `BOOST_ROOT`).</br>Linux는 `libboost-timer-dev`(방법 B 스크립트가 같이 설치함). |
| 카메라 드라이버 | Live 카메라 사용 시 | Windows: WinUSB([Quick Start 7번](#windows)). Linux: udev 규칙([Quick Start 6번](#linux-ubuntudebian-기준)). |

> 나중에 Boost 헤더를 쓰는 Metavision 헤더(예: `sdk/core`의 timing profiler, preprocessors)를 include하게 되면 컴파일 에러가 남. 그때는 [Boost 사전빌드 바이너리](https://sourceforge.net/projects/boost/files/boost-binaries/)를 설치(VS2019 → `msvc-14.2`, VS2022 → `msvc-14.3`).
>
> `vcpkg.json`이 리포에 있지만 현재 실제로 사용하는 패키지는 없음(vcpkg 불필요).

### 환경 변수

| 변수 | 필요 여부 | 설명 |
|---|---|---|
| `QT_DIR` | Qt를 기본 경로(`C:\Qt\...`) 밖에 설치했을 때만 | Qt 키트 경로(예: `C:\Qt\6.5.3\msvc2019_64`). 없으면 `QTDIR`을 대신 확인하고, 그것도 없으면 `C:\Qt\*\msvc*_64`를 자동 검색함. |
| `BOOST_ROOT` | 보통 불필요 | 실제 Boost를 `C:\local\boost_*`가 아닌 곳에 설치했고 그걸 쓰고 싶을 때만 지정. |
| `MV_HAL_PLUGIN_PATH` | 불필요 | 앱이 실행될 때 실행 파일 옆 `hal_plugins\`로 자동 설정함(`EnsureBundledHalPluginPath()`). 직접 설정하면 그 값이 우선됨. |
| `OpenCV_DIR` | 불필요 | 다른 OpenCV를 쓰고 싶을 때만 지정. |

### CMake 옵션

| 옵션 | 기본값 | 설명 |
|---|---|---|
| `EVENTCORE_NO_METAVISION` | `OFF` | `ON`이면 Metavision SDK를 쓰지 않음(CSV 입력만 지원, `EventProcessing.DiagQt` 제외). |
| `EVENTCORE_USE_SYSTEM_METAVISION` | `OFF` | `ON`이면 번들(`Prophesee-window/`, `Prophesee-linux/`) 대신 시스템에 설치된 SDK를 사용함. 번들 버전이 녹화된 RAW 포맷을 못 열 때 등에 사용. 번들 런타임 복사도 같이 꺼짐. |
| `CMAKE_BUILD_TYPE` | `Release` | Windows에서 Metavision SDK가 감지되면 Release로 강제됨([Release 전용](#release-전용-windows) 참고). |

### 알려진 이슈

- **VS2019에서 preset 선택이 무시되는 경우가 있음.** `windows-qt`가 아닌 다른 구성(예: VS 기본 `x64-Debug`)으로 configure될 수 있음.
  - 툴바 드롭다운에서 `windows-qt`를 직접 선택할 것.
  - 어떤 구성으로 configure되든 `CMakeLists.txt`가 Windows에서 Qt, 번들 OpenCV, 번들 Metavision SDK, Boost(또는 stub)를 자동으로 찾고 Release를 강제하므로 빌드 자체는 동작함.
- **UI 모양이 PC마다 다르게 보일 수 있음.** 코드에서 Qt 스타일을 지정하지 않으므로, 설치된 Qt 버전과 Windows 버전에 따라 기본 스타일이 달라짐(예: Qt 6.7+를 Windows 11에서 쓰면 `windows11` 스타일).

---

## Project Structure

```text
EventCameraProcessing/
├─ EventProcessing.Core/         # 이벤트 입력·처리 Static Library
│  └─ Calibration/               # Camera intrinsic calibration (README.md 포함)
├─ EventProcessing.Console/      # RAW / CSV / Live 입력 Batch Processing
├─ EventProcessing.DiagQt/       # Qt Widgets Live / RAW Diagnostic Viewer
├─ Prophesee-window/             # 번들 Metavision SDK 5.2.0 (Windows: include / lib / bin)
├─ Prophesee-linux/              # 번들 OpenEB 5.2.0 + 의존 .so, udev 규칙 (Linux)
├─ ocv440/                       # 번들 OpenCV 4.4.0 (Windows, + 루트 opencv_world440(d).dll)
├─ cmake/boost-stub/             # Boost가 없을 때 쓰는 빈 Boost CMake 패키지
├─ scripts/setup-linux-openeb.sh # Linux OpenEB 소스 빌드/설치 스크립트
├─ CMakeLists.txt, CMakePresets.json   # Windows / Linux 공용 빌드 (GUI 포함)
├─ EventCameraProcessing.sln, Metavision.props  # Windows MSBuild (Core·Console만)
└─ README.md
```

> GUI는 Qt Widgets(`EventProcessing.DiagQt`) 하나로 통일됨. 예전의 Windows 전용 MFC 버전(`EventProcessing.Diag`)은 제거됨.
> `EventCameraProcessing.sln`(MSBuild)은 Core와 Console만 포함함. GUI는 Windows에서도 CMake로만 빌드됨.

---

## EventProcessing.Core

이벤트 카메라 데이터 처리의 핵심 로직. `EventProcessing.Console`과 `EventProcessing.DiagQt`가 공통으로 사용함.

- 입력: CSV Event Stream, Metavision RAW File, EVK4 HD Live Camera (`IEventSource`, `EventSourceFactory`)
- Event Noise Filtering (`EventFilter`)
- Time-window Event Accumulation, Positive / Negative / Merged 시각화 (`EventAccumulator`, `EventVisualizer`)
- Ball Candidate Detection (`BallDetector`)
- Searching / Ready / Impact / Trajectory 상태 관리 (`ShotTrigger`)
- Camera Intrinsic Calibration (`Calibration/`, [Camera Calibration](#camera-calibration) 참고)

## EventProcessing.Console

Command-line Batch Processing. 입력 Event Stream을 일정 시간 구간으로 나눠 Accumulation Image와 Debug 결과를 만듦.

```bash
EventProcessing.Console <input.raw|input.csv|live> [outputDir=output] [windowUs=10000] [fps=30]
```

```text
RAW / CSV / Live → Event Source → Noise Filtering → Time-window Accumulation → Ball Detection → Image / Video Output
```

출력: Positive / Negative / Merged Event Image, Binary Mask, Ball Detection Debug Image, MP4 Visualization Video.

## EventProcessing.DiagQt

EVK4 HD Live Camera 또는 RAW Recording의 Event Stream을 실시간으로 확인하는 Qt Widgets 앱. Ball Detection 결과를 기반으로 Shot Capture를 수행하고, Camera Bias 조정과 Calibration 기능을 제공함.

### Shot Trigger 상태 머신

```text
Searching → (Ball 검출) → Ready → (방향 일관성 확인) → Impact → Trajectory (TRJCT) → Searching
```

- **Ready**: Ball이 `readySeconds` 이상 같은 위치(`stableMovePx` 이내)에서 안정적으로 검출되면 전환됨.
- **Impact**: Ready 상태에서 Ball이 움직이기 시작한 뒤 연속된 이동 벡터의 방향이 `directionConsistentFrames` 동안 `maxDirectionDeviationDeg` 이내로 유지되면 확정됨. 이 방식으로 지그재그 잡음과 실제 샷을 구분함. 트리거 시점은 확인이 끝난 프레임이 아니라 이동이 시작된 첫 프레임임.
- **Trajectory**: Impact 첫 프레임 기준으로 이전 `preCaptureSeconds`(기본 2초, 프레임 버퍼에서 소급 저장)와 이후 `postCaptureSeconds`(기본 2초)를 `shot_*` 폴더에 저장한 뒤 Searching으로 복귀함.
- RAW 재생도 같은 `EventProcessor::Process` 파이프라인을 거침. 매 프레임 Ball 추정 물체의 중심(초록 점)과 외곽(빨간 원 + 파란 바운딩 박스)이 표시됨.
- 상태가 바뀔 때마다 로그에 `State: <이전> -> <다음>`이 기록됨. 트리거 시에는 pre-roll 프레임 수가, Trajectory가 끝나면 저장된 프레임 수와 폴더 경로가 함께 기록됨.

### 컨트롤 버튼

- **Start / Stop**: Start는 Live 모드에서 "샷 감지 대기 시작"을 뜻하며, 바로 녹화하는 게 아님. Stop은 항상 완전히 종료하고 IDLE로 돌아감(미리보기도 초기화). 수동 녹화 중에 Stop을 누르면 녹화를 저장한 뒤 종료함.
- **Pause / Resume**: 실행 중에만 활성화됨.
  - RAW: 재생을 그 자리에서 멈추고, Resume하면 멈춘 시각부터 이어서 재생함.
  - Live: 미리보기는 계속 흐르고, ShotTrigger 갱신과 프레임 저장(자동 캡처 + 수동 녹화)만 멈춤.
- **Record / Save**: Live로 Start된 상태에서만 활성화됨. Record를 누르면 매 프레임을 `output/manual_<날짜시각>/`에 저장하기 시작하고, Save(또는 Stop)를 누르면 마무리함. 자동 샷 캡처와 독립적으로 동작함.

### Source 그룹 / 메뉴

- **Source 텍스트 박스**
  - RAW 모드: RAW 파일 경로를 표시하고 편집할 수 있음.
  - Live 모드: 읽기 전용. 연결되면 카메라 식별자를 표시함(`serial_number` 우선, `integrator`와 센서 세대가 있으면 괄호로 덧붙임). EVK4/IMX636은 MAC 주소 대신 시리얼 번호가 고유 식별자임.
- **File**: `Mode`(Live / RAW, Source 라디오 버튼과 동기화됨), `Open File...`, `Set Output Path...`.
- **Settings > Language**: 기본값은 한국어. English와 한국어를 즉시 전환함. 로그 메시지와 상태 코드(`SEARCHING`/`READY`/`IMPACT`/`TRJCT`/`IDLE`)는 항상 영어로 고정됨.
- **About**: `Version`은 빌드 시점의 git 커밋 해시와 날짜임. `License`는 제품, 저작권, 서드파티 라이선스(번들 SDK·HDF5 라이선스 파일 경로), 연락처를 보여줌. 정식 오픈소스 라이선스는 아직 지정되지 않음(LICENSE 파일 없음).

### Camera Bias 그룹

IMX636 표준 bias 6종의 의미(값을 올렸을 때의 동작 기준). 슬라이더 툴팁에도 같은 설명이 표시됨.

| Bias | 설명 | 기본값 |
|---|---|---|
| `bias_diff` | ON/OFF 임계값의 기준 변경 | -30 |
| `bias_diff_on` | 값 증가 시, 밝아지는 변화에 둔감 | -5 |
| `bias_diff_off` | 값 증가 시, 어두워지는 변화에 민감 | -15 |
| `bias_hpf` | 값 증가 시, 느린 밝기 변화에 둔감 | 카메라 보고 값 |
| `bias_fo` | 값 증가 시, 빠른 밝기 변화 감지 능력 증가 | 20 |
| `bias_refr` | 값 증가 시, 동일 픽셀에서 더 자주 이벤트 발생 | 카메라 보고 값 |

- **슬라이더**: IMX636 표준 bias 6종(`bias_diff`, `bias_diff_off`, `bias_diff_on` / `bias_fo`, `bias_hpf`, `bias_refr`)의 슬라이더가 2열로 표시됨.
  - 연결 전에는 비활성화되고 `--`로 표시됨.
  - Live Start에 성공하면 HAL `I_LL_Biases`에서 읽은 실제 값과 권장 범위로 활성화됨. 슬라이더를 움직이면 `I_LL_Biases::set()`으로 즉시 반영됨.
  - 수정 불가능한 bias(`is_modifiable()==false`)는 값만 표시함.
  - 알려진 6종 밖의 bias가 보고되면 그 행이 동적으로 추가됨.
- **기본값**: 위 표의 값이 처음 연결할 때 카메라 범위 안으로 clamp되어 적용됨. `bias_hpf`, `bias_refr`은 카메라가 보고한 현재 값을 그대로 사용함.
- **값 기억**: 사용자가 바꾼 값은 프로그램이 종료될 때까지 메모리에 기억되어 Start/Stop을 반복해도 다시 적용됨(디스크에는 저장하지 않음).
- **Save Bias... / Load Bias...**: Metavision 표준 `.bias` 파일로 저장하거나 불러옴(카메라 연결 시에만 활성화). 불러온 값은 기억 값으로도 채택됨.
- **툴팁**: 한국어 모드에서는 위 표의 설명을 보여줌. English 모드에서는 같은 뜻의 영어 설명을 보여주고, 카메라가 연결되면 HAL이 보고하는 원문으로 바뀜. 표에 없는 bias는 HAL 원문을 그대로 보여줌.

### Calibration 그룹

Calibration Mode, 누적 시간, 체커보드 설정, 샘플 캡처, 캘리브레이션 실행, 저장/불러오기를 제공함. 아래 [Camera Calibration](#camera-calibration) 참고.

---

## Camera Calibration

체커보드를 이용한 이벤트 카메라 intrinsic calibration(K, 왜곡 계수). 알고리즘은 `EventProcessing.Core/Calibration/`에 있음. OpenCV(core/imgproc/calib3d)만 사용하므로 Live, RAW, CSV 어느 입력에서든 같은 코드로 동작함. Calibration Mode에서는 BallDetector와 ShotTrigger가 실행되지 않으므로 기존 기능에 영향이 없음.

자세한 내용(파일별 역할, 검출 전략, 모델 선택 기준, 결과 해석, 저장 형식)은 **[EventProcessing.Core/Calibration/README.md](EventProcessing.Core/Calibration/README.md)** 참고.

### 파이프라인

```text
Event Stream (Live / RAW)
   → CalibrationImageBuilder     # Δt 동안 누적 → polarity 기반 2D 이미지
   → CheckerboardDetector        # 내부 코너 검출 (subpixel)
   → CalibrationSampleCollector  # 여러 pose의 observation 수집
   → CameraCalibrator            # cv::calibrateCamera → K, 왜곡, RMS, per-view 오차
   → CalibrationIO               # YAML 저장 / 불러오기
```

### 사용 방법 (DiagQt)

1. 카메라를 실제 사용 환경과 같은 위치에 고정함(카메라는 고정하고 체커보드를 움직임).
2. Source에서 Live camera 또는 RAW 파일을 선택.
3. **Calibration Mode**를 체크. 프리뷰가 누적 이미지로 바뀌고 Ball 검출과 Shot Trigger가 꺼짐.
4. **Rows / Columns / Square (mm)** 를 실제 보드에 맞게 입력함. 값은 칸 수가 아니라 **내부 코너 수**임. 예: 11×7 칸 보드면 내부 코너 10×6 → Columns=10, Rows=6(기본값).
5. **Accumulation (ms)** 를 조정함(초기값 50 ms, 50~100 ms 권장).
6. **Start**를 누른 뒤 체커보드를 여러 pose로 움직이고, `Detected: YES`일 때 **Capture Sample**을 누름.
   - 권장 pose: 정면, 좌·우·상·하 tilt, 네 모서리, 가까이, 멀리.
   - 최소 3장이 필요하지만 **10~20장 이상**을 화면 전체에 골고루 분포시키는 것이 좋음.
   - Capture한 샘플은 `output/calibration_<날짜시각>/sample_NNN_image.png`(누적 이미지), `sample_NNN_corners.png`(코너 오버레이), 경계 재구성으로 찾은 경우 `sample_NNN_reconstructed.png`로도 저장됨. 저장 경로는 로그에 표시됨.
7. **Run Calibration**을 누르면 로그에 fx/fy/cx/cy, 왜곡 계수, RMS, per-view 오차가 출력됨. 오차가 유독 큰 pose가 있으면 **Remove Last** / **Clear**로 정리한 뒤 다시 실행.
   계산이 성공하면 결과 `.yml`이 같은 `calibration_<날짜시각>/` 폴더에 자동 저장됨.
8. **Save Calibration...** 으로 다른 위치에도 `.yml`을 저장할 수 있음. **Load Calibration...** 으로 저장된 `.yml`을 다시 불러옴(대화상자는 위 폴더에서 열림).

> 수집한 샘플은 Start/Stop을 반복해도 유지됨(RAW를 반복 재생하면서 수집 가능). 다른 보드 설정(rows/cols/square)으로 캡처하려면 먼저 Clear해야 함.

### 검증 상태

Windows에서 빌드와 실행은 확인됨. 다만 아직 **실제 체커보드로 정확도를 실측 검증하지 않음**. 검증할 항목은 검출 안정성, K·왜곡 값의 타당성, RMS 수준, 저장/불러오기 왕복, pinhole과 fisheye 중 어느 모델이 맞는지임. 기본 모델은 pinhole(k1, k2, p1, p2, k3)이고 fisheye는 미구현.

---

## Build 구성 상세

### 번들 SDK 자동 감지

`CMakeLists.txt`(Windows/Linux)와 `Metavision.props`(MSBuild)는 리포에 번들된 SDK를 시스템 설치본보다 먼저 사용함. 그래서 clone만 하면 같은 빌드 환경이 재현됨.

| | Windows | Linux |
|---|---|---|
| OpenCV | `ocv440/lib` (calib3d 포함 `opencv_world440`) | 시스템 OpenCV |
| Metavision SDK | `Prophesee-window/` → 없으면 시스템 설치본(`C:\Program Files\Prophesee`) | `Prophesee-linux/` → 없으면 시스템 설치본 |
| Boost | `C:\local\boost_*` / `BOOST_ROOT` → 없으면 `cmake/boost-stub/` | 시스템 Boost |

### 런타임 배포

빌드가 끝나면 실행에 필요한 파일이 실행 파일 옆으로 자동 복사되므로, 시스템 `PATH`를 설정하지 않아도 바로 실행됨.

- **Windows**(`eventcore_copy_windows_runtime_deps`): 아래 파일을 복사함.
  - `Prophesee-window\bin\*.dll`
  - HAL 플러그인 → `hal_plugins\` (`hal_plugin_prophesee.dll`, `metavision_psee_hw_layer.dll`, `libusb-1.0.dll`)
  - `opencv_world440.dll`
  - 그 밖에 타깃이 링크한 DLL(`TARGET_RUNTIME_DLLS`, CMake 3.21+)
  - Qt DLL은 `windeployqt`로 배포함.
- **Linux**(`eventcore_copy_linux_runtime_deps`): `Prophesee-linux/lib`의 `.so`를 복사하고 RPATH를 `$ORIGIN`으로 설정함(`--disable-new-dtags`).
- **HAL 플러그인 경로**: 앱이 실행될 때 실행 파일 옆 `hal_plugins/`를 찾아 `MV_HAL_PLUGIN_PATH`를 자동으로 설정함.

### Release 전용 (Windows)

Prophesee는 Windows용 Metavision SDK를 **Release 바이너리로만** 배포함(디버그 DLL 없음). Debug(`/MDd`)로 빌드한 코드가 Release(`/MD`) DLL과 C++ 예외나 STL 객체를 주고받으면 ABI가 맞지 않아 크래시가 남(예: RAW 파일 열기 실패 시 액세스 위반). 그래서 Metavision SDK가 감지되면 CMake가 Release 구성만 허용함.

---

## Research Direction

현재 목적은 최종 Spin Estimation 알고리즘을 바로 구현하는 것이 아님. 실제 Event Camera Data를 취득·확인하고, 후속 연구에 쓸 Event Processing Pipeline을 구축하는 것이 목적임.

```text
Event Acquisition → Data Validation → Visualization → Noise Filtering → Ball ROI Detection
  → Ball Tracking → Event-based Optical Flow → Dimple / Surface Feature Tracking
  → Rotation Estimation → Spin Estimation
```

- **Ball Tracking**: 현재의 Contour 기반 Ball Candidate Detection을, 시간적으로 연속된 위치를 추적하는 방식으로 확장할 예정. 이를 통해 Club, Shoe, Player, Background Noise와 Ball을 구분.
- **Event-based Optical Flow**: Accumulation Image 대신 각 Event의 microsecond 단위 timestamp `(x, y, t, polarity)`를 이용한 Motion Estimation을 검토.
- **Dimple / Surface Feature Tracking**: 골프공 표면의 Dimple과 Texture가 만드는 Event Pattern으로 Surface Motion을 추적. Event Camera의 높은 시간 해상도로 Motion Blur와 Temporal Aliasing을 줄일 수 있는지 분석할 예정.
- **Rotation / Spin Estimation**: Surface Motion과 Optical Flow로 Rotation Axis와 Angular Velocity를 추정하는 방향으로 확장. Temporal Feature Tracking, Ball Geometry Constraint, Camera Calibration, Rotation Model, Physics-based Constraint를 검토할 예정.

Accumulation Image는 Visualization과 Debugging 용도로 쓰고, 최종 Motion / Spin Estimation에서는 원본 Event Timestamp 정보를 최대한 유지할 예정.

---

## Current Status

### Implemented

- [x] CSV / Metavision RAW / EVK4 HD Live 입력
- [x] Time-window Accumulation, Positive / Negative / Merged Visualization
- [x] Basic Noise Filtering, Binary Mask, Basic Ball Candidate Detection
- [x] Debug Image, PNG, MP4 출력 / Console Batch Processing
- [x] Qt Diagnostic Viewer (Windows / Linux), English / 한국어 UI
- [x] Searching / Ready / Impact / Trajectory 상태 머신 (방향 일관성 트리거, pre/post capture, 수동 녹화)
- [x] Live Camera Bias 조정 및 `.bias` 저장/불러오기
- [x] Camera Intrinsic Calibration (누적 이미지 → 체커보드 검출 → `calibrateCamera` → YAML 저장/불러오기). 실측 검증은 아직 안 됨
- [x] CMake Build (Windows / Linux), 번들 SDK 자동 감지, 런타임 DLL/.so 자동 배포, HAL Plugin 경로 자동 설정

### In Progress / Next

- [ ] Calibration 실측 검증 (체커보드 실촬영, RMS, pinhole/fisheye 판단)
- [ ] Real-world Event Data Quality Evaluation
- [ ] Camera Bias Setting Validation
- [ ] Event Noise Filtering 개선
- [ ] Ball Detection Robustness 개선 / Temporal Ball Tracking / Ball ROI Stabilization
- [ ] Event-based Optical Flow / Motion Compensation
- [ ] Dimple / Surface Feature Tracking
- [ ] Rotation Axis / Angular Velocity / Golf Ball Spin Estimation
- [ ] Calibration 결과를 Tracking·Spin 추정에 적용
- [ ] Physics-based Motion Constraint Integration

```text
Acquisition / RAW·Live Loading / Visualization / Basic Detection / Calibration(구현)  [Done]
        ↓
Calibration 실측 검증 / Robust Tracking / Optical Flow                             [Next]
        ↓
Rotation / Spin                                                                     [Future]
```

---

## Current Limitations

### Ball Detection Robustness

`BallDetector`는 Accumulation Image에서 Contour를 분석해 Ball Candidate를 찾는 기본 방식임. 실제 스윙 데이터에서는 Club, Shoe, Player, Background Edge, Lighting Noise에서도 Event가 많이 발생함. 그래서 가장 큰 Contour나 단순한 Shape 조건만으로는 다른 객체가 Ball로 검출될 수 있음.
현재 Ball Detection은 **Data Validation과 Trigger Logic 검증용 Prototype**이고, Temporal Tracking과 ROI 기반 검출을 추가해야 함.

### Event Noise

Sensor Bias, Lighting, Reflection, Background Activity에 따라 Noise Event가 발생함. 현재는 Basic Filtering 단계임. Background Activity / Temporal / Spatial Filtering, Hot Pixel Removal, ROI / Motion 기반 Filtering을 검토할 예정.

### Event Accumulation Window

Accumulation Image의 품질은 `windowUs`에 따라 달라짐.
- Window가 너무 작으면 Event 수가 부족해 Ball 형태가 잘 드러나지 않음.
- Window가 너무 크면 빠른 Motion이 한 Frame에 겹쳐 Ball Event가 퍼짐.

Camera Setting, Ball Speed, 분석 목적에 맞는 값을 골라야 함. Calibration의 `Accumulation (ms)`도 같은 trade-off가 있음.

### Camera Bias Dependency

Bias가 적절하지 않으면 Background Noise와 Event Rate가 지나치게 늘거나, Ball Surface Event와 Feature Contrast가 부족해짐. DiagQt의 Camera Bias 그룹에서 실시간으로 조절할 수 있지만, 적절한 값은 Lighting, Camera Position, Ball Distance, Lens, Background에 따라 반복 실험으로 찾아야 함.

### Accumulation Image Information Loss

Accumulation은 여러 Timestamp의 Event를 한 Frame으로 합치므로 Temporal Information 일부가 사라짐(`(x, y, t, polarity)` → `(x, y, intensity)`). 후속 Optical Flow와 Spin Estimation에서는 원본 Event Timestamp를 직접 활용하는 처리를 병행해야 함.

### Spin Estimation

최종 Golf Ball Spin Estimation은 아직 구현되지 않음. 현재 범위는 Acquisition → Visualization → Filtering → Ball Detection → Capture, 그리고 Calibration까지임. 이후 단계는 [Research Direction](#research-direction) 참고.
