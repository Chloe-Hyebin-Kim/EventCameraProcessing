#pragma once

// 윈도우별 측정값 CSV 로거.
//
// 왜 필요한가:
//   지금 파이프라인의 출력은 PNG와 mp4뿐이다. Phase 0(임계 각해상도 검증)은
//   "공 지름 px에 따라 스핀 추정 오차가 어떻게 변하는가" 곡선을 그려야 하는 실험인데,
//   그림만 나오고 숫자가 남지 않으면 아무것도 그릴 수 없다.
//
//   여기서는 윈도우마다 한 줄씩 다음을 남긴다:
//     - 시간 구간
//     - ON / OFF 이벤트 수 (Phase 0의 핵심 관측량. 딤플이 이벤트를 만드는지를 직접 센다)
//     - 검출 여부와 서브픽셀 중심/반지름
//     - 형상 지표(원형도, 종횡비)와 적합 품질(rms, coverage)
//     - ShotTrigger 상태
//
//   scripts/analyze_shot.py 가 이 CSV를 그대로 읽어 그래프를 그린다.

#include <fstream>
#include <string>

#include "Event.h"

namespace eventcore
{
    struct ShotMeasurement
    {
        lli windowStartUs = 0;
        lli windowEndUs = 0;

        int eventCount = 0;
        int onCount = 0;            // polarity > 0
        int offCount = 0;           // polarity <= 0

        bool detected = false;
        double centerX = 0.0;       // 서브픽셀
        double centerY = 0.0;
        double radiusPx = 0.0;      // 서브픽셀
        double momentCentroidX = 0.0;
        double momentCentroidY = 0.0;

        double areaPx = 0.0;
        double circularity = 0.0;
        double aspectRatio = 0.0;

        double fitRmsPx = 0.0;
        double fitCoverage = 0.0;   // 반지름 신뢰도의 실제 판단 근거. CircleFit.h 주석 참조
        bool radiusTrusted = false;
        int fitPointsUsed = 0;
        int fitPointsTotal = 0;

        int contoursTotal = 0;
        int contoursPassed = 0;
        std::string rejectReason;   // RobustBallDetector::RejectReasonName()

        int shotState = 0;          // ShotState를 int로 캐스팅한 값
        bool justEnteredReady = false;
        bool justTriggered = false;

        // 직전 검출과의 중심 이동 속도(px/s). 검출이 없거나 직전 값이 없으면 -1.
        double centerSpeedPxPerSec = -1.0;
    };

    class ShotLogger
    {
    public:
        ShotLogger() = default;
        ~ShotLogger();

        ShotLogger(const ShotLogger&) = delete;
        ShotLogger& operator=(const ShotLogger&) = delete;

        // 파일을 만들고 헤더를 쓴다. 이미 열려 있으면 먼저 닫는다.
        bool Open(const std::string& path);

        void Append(const ShotMeasurement& m);

        void Close();

        bool IsOpen() const { return m_open; }
        const std::string& Path() const { return m_path; }
        long long RowsWritten() const { return m_rows; }

        static const char* HeaderLine();

    private:
        std::ofstream m_file;
        bool m_open = false;
        std::string m_path;
        long long m_rows = 0;
    };
}
