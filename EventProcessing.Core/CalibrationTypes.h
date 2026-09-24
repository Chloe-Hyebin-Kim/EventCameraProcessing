#pragma once

#include "Event.h"  // eventcore::lli

namespace eventcore
{
    // Event Camera Intrinsic Calibration 파이프라인이 공통으로 쓰는 설정/자료형 모음.
    // (Phase가 진행되면서 checkerboard 설정, observation, calibration 결과 자료형이 여기에 추가된다.
    //  현재 Phase 1에서는 calibration 이미지 생성 설정만 정의한다.)

    // 일정 temporal window [t0, t0+Δt) 동안 event를 누적해 만드는 calibration용 2D 이미지 설정.
    //
    // 첫 구현에서는 neural-network 기반 intensity reconstruction 같은 복잡한 방법을 쓰지 않고,
    // polarity 기반의 단순한 누적 이미지를 만든다:
    //   해당 픽셀에서 관측된 polarity가 우세한 쪽에 따라
    //     Positive 우세 -> positiveValue (기본 255)
    //     Negative 우세 -> negativeValue (기본 0)
    //     이벤트 없음/동수 -> noEventValue (기본 127)
    struct CalibrationImageConfig
    {
        // Δt: 하나의 calibration 이미지에 누적할 시간(microseconds).
        // 초기 기본값 50 ms는 실험을 위한 시작값일 뿐이며, GUI/설정에서 변경 가능하다(하드코딩 아님).
        // 50~100 ms 수준을 권장하되 이는 렌즈/조명/checkerboard 이동 속도에 따라 조정한다.
        lli accumulationUs = 50000;

        int positiveValue = 255;  // 양극성(ON) 이벤트가 우세한 픽셀 값
        int negativeValue = 0;    // 음극성(OFF) 이벤트가 우세한 픽셀 값
        int noEventValue = 127;   // 이벤트가 없거나 양/음 이벤트 수가 같은 픽셀 값
    };

    // Checkerboard(체스판) 패턴 설정. OpenCV의 patternSize는 "내부 코너" 개수를 뜻하며
    // cv::Size(가로 내부 코너 수, 세로 내부 코너 수) = cv::Size(innerCornerCols, innerCornerRows)로
    // 매핑된다(검출기 쪽에서 이 규칙으로 변환한다).
    //
    // 예: 10x7 칸(square)짜리 체스판이면 내부 코너는 9x6 -> innerCornerCols=9, innerCornerRows=6.
    struct CheckerboardConfig
    {
        int innerCornerRows = 6;     // 세로 방향 내부 코너 수
        int innerCornerCols = 9;     // 가로 방향 내부 코너 수
        double squareSizeMm = 25.0;  // 한 칸(square)의 실제 한 변 길이(mm). intrinsic 계산 스케일에 사용.
    };
}
