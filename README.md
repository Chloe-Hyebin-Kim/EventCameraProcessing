## Project Structure

프로젝트는 이벤트 입력 및 처리 로직을 담당하는 Core Library, Batch Processing용 Console Application, 실시간 확인을 위한 Qt5 Diagnostic Application으로 구성함. GUI와 빌드 시스템은 Windows와 Linux에서 동일한 소스를 사용함.
<img width="871" height="605" alt="image" src="https://github.com/user-attachments/assets/f8ddd6cd-e0b1-4ad6-85cc-ce4898637d88" />

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
├─ EventProcessing.Diag/
│  └─ Qt5 기반 Live / RAW Diagnostic Viewer
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
├─ EventCameraProcessing.sln
└─ README.md
```

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

`EventProcessing.Console`, `EventProcessing.Diag`에서 공통으로 `EventProcessing.Core`를 사용함.

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

### EventProcessing.Diag

Qt5 Widgets 기반 크로스 플랫폼 Diagnostic Application.

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
Platform        : Windows x64 / Linux x86_64
GUI             : Qt 5 Widgets
Build           : CMake 3.16+
```

### Windows / Linux 빌드

OpenCV 4, Qt 5 Widgets와 (RAW/Live 입력이 필요하면) 플랫폼에 맞는 Metavision SDK 5.x를 설치한다. 저장소에 포함된 `Prophesee` 바이너리는 Windows용이므로 Linux에서는 Prophesee가 제공하는 Linux SDK를 설치하고 `METAVISION_SDK_PATH`를 지정해야 한다.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Linux에서 실행:

```bash
./build/CEventProcessingDiagDlg
```

Windows에서는 Visual Studio의 **CMake 프로젝트 열기**를 사용하거나 같은 명령을 Developer PowerShell에서 실행한 뒤 `build/Release/CEventProcessingDiagDlg.exe`를 실행한다. Qt/OpenCV가 기본 검색 경로에 없다면 `CMAKE_PREFIX_PATH`를 설치 경로로 지정한다.

Metavision SDK 없이 UI와 CSV 기반 Core만 빌드할 수도 있다. 이 경우 Diagnostic 앱은 실행되지만 RAW/Live 시작 시 SDK 미지원 메시지를 표시한다.

```bash
cmake -S . -B build -DEVENTPROCESSING_WITH_METAVISION=OFF
cmake --build build --parallel
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

`EventProcessing.Console`, `EventProcessing.Diag`에 Post-Build Copy Step 적용.

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
EventProcessing.Diag.exe
│
├─ Metavision Runtime DLLs
│
└─ hal_plugins/
   ├─ hal_plugin_prophesee.dll
   ├─ metavision_psee_hw_layer.dll
   └─ libusb-1.0.dll
```

별도의 System-wide HAL Plugin Path 설정 없이 실행할 수 있도록 구성함.

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
- [x] Qt5 Cross-platform Diagnostic Viewer
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
