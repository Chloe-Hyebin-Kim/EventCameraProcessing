# Event Camera Intrinsic Calibration — 사용법 & 파이프라인

> 대상: Prophesee Metavision EVK4HD (IMX636, 1280×720)
> 위치: `EventProcessing.Core/Calibration/`(알고리즘) + `EventProcessing.DiagQt`(GUI)
> 빌드 방법은 [루트 README](../../README.md) 참고

## 1. 파이프라인 개요

    EVK4HD Event Stream (Live) / RAW playback
            ↓  (원본 event 배치: x, y, t, polarity)
    CalibrationImageBuilder      # Δt 동안 누적 → polarity 기반 2D 이미지
            ↓
    EdgeCheckerboardReconstructor  # (움직이는 보드) 경계 띠 → 채워진 체커보드 재구성
            ↓
    CheckerboardDetector         # 체커보드 내부 코너 검출 (subpixel)
            ↓
    CalibrationSampleCollector   # 여러 pose의 observation 수집
            ↓
    CameraCalibrator             # cv::calibrateCamera → K, 왜곡, RMS
            ↓
    CalibrationIO                # YAML 파일로 저장 / 불러오기

- calibration 알고리즘은 카메라 입력과 분리되어 있어 **Live / RAW / CSV** 어디서든 같은 코드로 동작한다.
- Metavision SDK / Qt에 의존하지 않고 OpenCV(core/imgproc/calib3d)만 사용한다.
- Calibration Mode에서는 기존 BallDetector / ShotTrigger 경로를 실행하지 않는다(기존 기능 불변).

## 2. 구성 요소 (파일 / 클래스)

| 파일 | 역할 |
|---|---|
| `CalibrationTypes.h` | `CalibrationImageConfig`(누적 Δt, polarity 픽셀값), `CheckerboardConfig`(내부 코너 rows/cols, square mm) |
| `CalibrationImageBuilder.{h,cpp}` | event를 Δt 동안 누적 → `CV_8UC1` 이미지(양극 255 / 음극 0 / 없음·동수 127) |
| `EdgeCheckerboardReconstructor.{h,cpp}` | 움직이는 체커보드의 event 개수 이미지(경계 띠)에서 칸을 분할하고 2색으로 칠해 "채워진" 체커보드 이미지를 재구성 |
| `CheckerboardDetector.{h,cpp}` | `findChessboardCornersSB`(우선) + 고전 `findChessboardCorners`+`cornerSubPix`(폴백). `thorough` 플래그로 속도/정확도 선택 |
| `CalibrationObservation.{h,cpp}` | `CalibrationObservation`(objectPoints/imagePoints/timestamp), `BuildObjectPoints()`, `CalibrationSampleCollector` |
| `CameraCalibrator.{h,cpp}` | `cv::calibrateCamera` 실행 → `CalibrationResult`(K, 왜곡, RMS, per-view 오차, rvecs/tvecs) |
| `CalibrationIO.{h,cpp}` | `cv::FileStorage`로 결과 저장/불러오기(YAML/XML) |

## 3. GUI 사용법 (EventProcessing.DiagQt)

"Calibration" 그룹에 다음 UI가 있다:

- `Calibration Mode` 체크박스
- `Accumulation (ms)` — 누적 시간 Δt
- `Checkerboard`: `Rows`, `Columns`(내부 코너 수), `Square (mm)`
- `Detected: YES/NO ...` 상태 라벨
- 버튼: `Capture Sample` / `Remove Last` / `Clear` / `Run Calibration` / `Save Calibration...` / `Load Calibration...`
- `Samples: N` 카운트

### 절차

1. 카메라를 **실제 사용 환경과 동일하게 고정**한다(카메라는 고정, 체커보드를 움직임).
2. Source에서 `Live camera`(또는 RAW 파일)를 선택.
3. `Calibration Mode` 체크 → BallDetector/ShotTrigger가 꺼지고, 프리뷰가 누적 이미지로 바뀐다.
4. `Rows` / `Columns` / `Square (mm)`를 **실제 보드**에 맞게 입력.
   - 예: 11×7 칸 보드 → 내부 코너 10×6 → Columns=10, Rows=6(기본값).
5. `Accumulation (ms)`를 조정(초기 50 ms 권장). 움직이는 체커보드의 edge가 안정적으로 보이도록 튜닝.
6. `Start`.
7. 체커보드를 여러 pose로 움직이며 `Detected: YES`일 때 `Capture Sample`.
   - **보드는 대각선 방향(가로+세로 성분이 모두 있게)으로 움직인다.** 한 축 방향으로만 움직이면 그 방향과 평행한
     경계에서 event가 나오지 않아 칸이 닫히지 않고, 검출되지 않는다(아래 5절 참고).
   - 크게 회전시키지 않는다. 회전이 커서 보드 일부가 거의 멈춰 있으면 그 부분 칸이 합쳐져 실패할 수 있다.
   - 너무 빨리 움직이지 않는다. Δt 동안 이동 거리가 칸 한 변에 가까워지면 칸이 사라진다.
   - 상태 라벨에 검출 경로가 표시된다: `경계 재구성`(움직이는 보드) / `극성 이미지`(깜빡이는 패턴 등).
   - 권장 pose: 정면 / 좌·우 tilt / 상·하 tilt / 네 모서리(좌상·우상·좌하·우하) / 가까이 / 멀리.
   - 최소 3장(기술적 최소)이지만 **10~20장 이상**, 화면 전체에 골고루 분포시키는 것이 좋다.
8. `Run Calibration` → 로그에 fx/fy/cx/cy, 왜곡, RMS, per-view 오차 출력.
9. per-view 오차에서 유독 큰 pose가 있으면 확인 후 `Remove Last` 등으로 정리하고 재실행(자동 제거는 하지 않음).
10. `Save Calibration...` → `.yml`로 저장.

메모:
- 수집한 샘플은 Start/Stop을 반복해도 세션 동안 유지된다(RAW 반복 재생으로도 수집 가능). `Clear`로만 비운다.
- 다른 rows/cols/square로 캡처하려 하면 거부된다(먼저 `Clear` 필요) — 서로 다른 보드를 섞으면 안 되므로.

## 4. 설정 항목 & 기본값

| 항목 | 기본값 | 비고 |
|---|---|---|
| Accumulation Δt | 50 ms | 실험용 초기값, 변경 가능(하드코딩 아님). 50~100 ms 권장 |
| Checkerboard Columns(내부 코너, 가로) | 10 | `cv::Size(cols, rows)`로 매핑 |
| Checkerboard Rows(내부 코너, 세로) | 6 | |
| Square size | 35 mm | intrinsic 스케일에 사용 |
| polarity 이미지 값 | +255 / −0 / 없음 127 | `CalibrationImageConfig` |

## 5. 검출 전략

### 왜 polarity 이미지만으로는 안 되는가

event camera는 밝기가 변하는 곳에서만 event를 낸다. 인쇄한 체커보드를 움직이면 **칸의 경계에만** event가 생기고
칸 내부는 비어(127) 있어, polarity 이미지가 "윤곽선"이 된다. `findChessboardCorners*`는 흑백으로 채워진 칸을
전제로 하므로 이 이미지에서는 검출되지 않는다(시뮬레이션으로 확인).

### 경계 재구성 (`EdgeCheckerboardReconstructor`)

1. 픽셀별 event 유무의 5×5 국소 밀도를, 이미지 전체 중앙값(=노이즈 밀도 추정)과 1의 중간값으로 이진화 → 경계 띠 마스크. closing으로 작은 끊김을 메움.
2. 경계가 아닌 영역의 connected component 중 면적이 비슷한 것을 칸으로 본다.
3. 경계 띠 픽셀을 가장 가까운 영역에 배정(Voronoi, `distanceTransform` label)해 영역 쌍별 공유 경계 길이를 잰다.
   변으로 맞닿은 칸은 경계가 길고, 대각선 칸은 한 점에서만 만난다.
4. 공유 경계가 긴 순서로 패리티 union-find를 돌려 "변 이웃 = 다른 색"으로 2색을 칠한다.
   보드 바깥 흰 여백(바깥 줄 흰 칸과 합쳐짐)도 같은 그래프에서 색이 정해진다.
5. 경계 띠를 배정된 영역의 색으로 채운 이미지에서 기존 `Detect()`를 실행한다. 코너는 띠 중앙선(누적 구간 중간 무렵의 경계 위치)에 놓인다.

흑백이 실제와 뒤바뀔 수 있으나 OpenCV 체커보드 검출은 반전에 무관하다.

### 순서 (`CheckerboardDetector::DetectEventImages`)

1. 경계 재구성 이미지에서 검출. 칸 전체에 event가 있는 경우(깜빡이는 패턴)에는 칸 후보가 없어 재구성이 곧바로 실패한다.
2. 실패하면 기존 polarity 이미지에서 검출(깜빡이는 패턴 등).

- **라이브 오버레이(매 Δt)**: 가벼운 `findChessboardCornersSB`(NORMALIZE만). 경계 재구성은 이미지 폭이 1000 px 이상이면 절반 해상도로 먼저 시도하고, 실패하면 원 해상도로 다시 시도한다.
- **Capture 시(1회)**: 원 해상도에서 정밀 검출 `findChessboardCornersSB`(EXHAUSTIVE|ACCURACY) → 실패 시 고전 `findChessboardCorners`+`cornerSubPix` 폴백. 저장되는 코너는 이 정밀 검출 결과.

### 시뮬레이션 검증 결과 (실기기 아님)

1280×720, 내부 코너 9×6, 칸 60 px, Δt 50 ms. log 밝기 변화가 임계값(0.25)을 넘을 때 event를 내는 단순 모델에 무작위 노이즈 event를 더했다.
시뮬레이션 event를 실제 `CalibrationImageBuilder` → `DetectEventImages`(C++, OpenCV 4.6)에 넣어 측정했다.
코너 오차 = 누적 구간 중간 시점의 실제 코너 위치와의 평균 거리.

| 움직임 | Capture 검출 | 평균 코너 오차 |
|---|---|---|
| 가로로만 300 px/s | 실패 (원리상 불가) | - |
| 대각선 100 / 300 / 1000 px/s | 성공 | 0.11 / 0.13 / 0.41 px |
| 가로 500 + 세로 100 px/s | 성공 | 0.20 px |
| 원근(기울임) + 대각선 400 px/s | 성공 | 0.22 px |
| 0.5 rad/s 회전 + 대각선 300 px/s | 성공 | 0.23 px |
| 대각선 100 px/s + 노이즈 3배 | 성공 | 0.39 px |
| 칸 30 px + 대각선 300 px/s | 성공 | 0.14 px |
| 깜빡이는 패턴(정지, 회귀 확인) | 성공 (polarity 경로) | 0.71 px |
| 보드 없음(노이즈만) | 미검출 (오검출 없음) | - |

Python 프로토타입에서는 1.0 rad/s 이상 회전 + 300 px/s 이동 조합이 실패했다(보드 일부의 국소 움직임이 0에 가까워져 칸이 합쳐짐).

**주의**: 모션 블러 때문에 코너는 누적 구간 동안의 평균 위치에 가깝다. 실기기에서는 센서 노이즈, bias, 조명, 렌즈 블러 때문에 결과가 다를 수 있으며 아직 검증되지 않았다.
`Run Calibration`의 RMS와 per-view 오차로 반드시 확인해야 한다.

## 6. Calibration 모델 (pinhole vs fisheye)

- **기본은 일반 pinhole 모델**: radial `k1,k2,k3` + tangential `p1,p2` (`cv::calibrateCamera`).
- EVK4HD는 렌즈 교체형이라 화각이 렌즈에 따라 다르다. **fisheye는 근거 없이 적용하지 않는다.**
- 판단 기준: pinhole로 RMS/ per-view 오차가 낮으면 pinhole로 충분. 오차가 크고 **특히 화면 가장자리 pose에서 체계적으로 커지면** 강한 배럴 왜곡 신호이므로 그때 fisheye(`cv::fisheye`) 도입을 검토(현재 코드에 자리만 마련, 미구현).

## 7. 결과 해석

`Run Calibration` 출력:

- `fx, fy` : 초점거리(픽셀). 정사각 픽셀이면 fx ≈ fy.
- `cx, cy` : 주점(픽셀). 보통 이미지 중심(≈640, ≈360) 근처.
- `k1,k2,p1,p2,k3` : 왜곡 계수.
- `RMS reprojection error` (px) : 전체 적합도. 낮을수록 좋음.
- per-view error : view별 재투영 RMS(px) + 코너 수 + timestamp. 특정 pose 이상 여부 진단용.

## 8. 저장 형식 (OpenCV FileStorage, YAML)

    image_width: 1280
    image_height: 720
    camera_matrix:
       fx: ...
       fy: ...
       cx: ...
       cy: ...
    distortion_coefficients:
       k1: ...
       k2: ...
       p1: ...
       p2: ...
       k3: ...
    rms_reprojection_error: ...
    checkerboard:
       rows: 6
       columns: 10
       square_size: 35.
    number_of_observations: ...
    model: pinhole

- `Load Calibration...`로 다시 읽으면 named 필드에서 3×3 K와 5×1 왜곡벡터를 재구성한다(undistort 등에 바로 사용 가능).
- 기존 프로젝트에 config 포맷이 없어 OpenCV로 왕복 호환되는 FileStorage(YAML)를 선택함.

## 9. 빌드 요구사항

- OpenCV **4.4.0** 기준으로 작성/검증(리포 번들 `ocv440/` 헤더).
- calib3d 모듈이 필요(`findChessboardCorners*`, `calibrateCamera`, `projectPoints`, `FileStorage`).
  - **Windows**: 번들 `opencv_world440`에 calib3d 포함 → 그대로 빌드됨.
  - **Linux**: 시스템 OpenCV(`sudo apt install libopencv-dev`, calib3d 포함)를 사용함. 번들 `Prophesee-linux/`의 OpenCV(4.2)는 core/imgproc/imgcodecs/videoio만 있어 calib3d가 없지만, `CMakeLists.txt`는 시스템 OpenCV를 쓰므로 문제없음.
  - calib3d가 없는 OpenCV가 잡히면 런타임이 아니라 configure 단계에서 바로 에러로 알려줌.

## 10. 검증 상태 (중요)

- **확인됨**: 코드가 Windows에서 **빌드·실행**된다(라이브 프리뷰/기존 기능 동작). Core 모듈은 OpenCV 4.4.0 헤더 대상 **컴파일** 확인.
- **시뮬레이션으로만 확인**: 움직이는 체커보드 경계 재구성 검출(5절 표). 실기기 검출률/정확도는 미검증.
- **아직 검증 안 됨**: 실제 **체커보드가 없어 Phase 2~6의 정확도(검출률/K·왜곡 값/RMS/저장·로딩 왕복)를 실측으로 검증하지 못함.**
- 따라서 아래는 실기기 검증 전까지 "정상 동작"으로 단정하지 않는다:
  - 이벤트 누적 이미지에서 체커보드 검출이 안정적인지
  - calibration 결과 값의 타당성 / RMS 수준
  - pinhole vs fisheye 최종 판단
- 권장 검증: 체커보드 확보 후 10~20 pose 수집 → Run Calibration → RMS·per-view 확인 → Save/Load 왕복 확인.
