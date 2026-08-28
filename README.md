# EventCameraProcessing

프로젝트는 이벤트 입력 및 처리 로직을 담당하는 Core Library, Batch Processing용 Console Application, 실시간 확인을 위한 Qt 기반 Diagnostic Application(Windows / Linux 공용)으로 구성함.
<img width="871" height="605" alt="image" src="https://github.com/user-attachments/assets/f8ddd6cd-e0b1-4ad6-85cc-ce4898637d88" />


</br>
</br>
</br>


## Quick Start (소스 받아서 GUI 실행하기)

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

3. **Metavision SDK(OpenEB) 빌드/설치** — 한 번만 하면 됨. OpenEB 자체가 커서 시간이 좀 걸림.
   ```bash
   ./scripts/setup-linux-openeb.sh          # /usr/local에 설치 (sudo 필요)
   # sudo 권한이 없으면:
   ./scripts/setup-linux-openeb.sh --user   # ~/.local/openeb에 설치 (완료 후 안내되는 환경변수 추가 필요)
   ```

4. **Configure + 빌드**
   ```bash
   cmake --preset linux
   cmake --build build/linux -j$(nproc)
   ```
   configure 로그에 `Metavision SDK found - Live camera / RAW support enabled` 가 보여야 `EventProcessing.DiagQt`까지 같이 빌드됨. 안 보이면 3번 스크립트를 아직 안 돌렸거나 새 셸을 안 열었을 가능성이 큼.

5. **실행**
   ```bash
   ./build/linux/EventProcessing.DiagQt/EventProcessing.DiagQt
   ```
   `error while loading shared libraries: libmetavision_sdk_core.so.5` 같은 에러가 뜨면 `sudo ldconfig` 한 번 실행 후 다시 시도.

       
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
