#pragma once

#include <vector>
#include <opencv2/opencv.hpp>

#include "Event.h"
#include "CalibrationTypes.h"

namespace eventcore
{
    // Event 스트림을 Δt(config.accumulationUs) 동안 누적해 polarity 기반 2D calibration 이미지를
    // 만드는 상태기(state machine).
    //
    // 설계 의도(요구사항: calibration algorithm을 camera input과 분리):
    //  - 입력은 오직 std::vector<Event> 배치이다. Live EVK4HD, RAW 재생, CSV 등 event가 어디서
    //    왔는지와 무관하게 동일하게 동작하므로, 카메라 없이 RAW/CSV만으로도 반복 테스트할 수 있다.
    //  - Metavision SDK나 Qt에 의존하지 않는다(OpenCV core/imgproc만 사용). 따라서 Metavision SDK가
    //    없는 빌드에서도 EventProcessing.Core의 일부로 컴파일된다.
    //
    // 사용 방식:
    //  - AddEvents()에 처리 윈도우 단위(예: LiveEventStream의 windowUs 배치)로 event를 계속 넣는다.
    //  - 누적된 시간 span(윈도우 timestamp 기준)이 accumulationUs에 도달하면 AddEvents()가 true를
    //    반환하고, 그 순간 Image()에 완성된 CV_8UC1 이미지가 준비된다. 그 뒤 내부 누적 버퍼는
    //    자동으로 리셋되어 다음 이미지를 위한 누적이 시작된다.
    //
    // 주: 기존 EventAccumulator(단일 윈도우용, count를 normalize)와 달리, 이 클래스는 여러 윈도우를
    //     가로질러 Δt만큼 누적한 뒤 polarity 우세도로 3-레벨(255/0/127) 이미지를 만든다.
    //     checkerboard의 움직이는 edge를 안정적으로 보이게 하는 것이 목적이라 normalize 대신
    //     polarity 기반 매핑을 쓴다.
    class CalibrationImageBuilder
    {
    public:
        // width/height는 센서 해상도(EVK4HD/IMX636 = 1280x720). 둘 다 > 0 이어야 한다.
        CalibrationImageBuilder(const CalibrationImageConfig& config, int width, int height);

        // 한 처리 윈도우의 event를 누적한다. windowStartUs/windowEndUs는 그 윈도우의 시간 범위이며,
        // 누적 시간 span을 재는 데 쓰인다(event가 비어 있는 윈도우도 시간은 흐르므로 함께 반영).
        // 누적 span이 accumulationUs 이상이 되면 이미지를 완성하고 true를 반환한다(그 뒤 자동 리셋).
        bool AddEvents(const std::vector<Event>& events, lli windowStartUs, lli windowEndUs);

        // 마지막으로 완성된 calibration 이미지(CV_8UC1). 아직 한 번도 완성되지 않았으면 비어 있다.
        const cv::Mat& Image() const { return m_image; }

        // 현재까지 누적된 시간(microseconds). 아직 누적을 시작하지 않았으면 0.
        lli CurrentSpanUs() const;

        // 누적 버퍼와 시간 span을 비운다(완성된 Image()는 그대로 둔다).
        void Reset();

        // 설정을 바꾸고 누적을 리셋한다(accumulationUs 등 변경 시).
        void SetConfig(const CalibrationImageConfig& config);
        const CalibrationImageConfig& Config() const { return m_config; }

        int Width() const { return m_width; }
        int Height() const { return m_height; }

    private:
        void Render();

        CalibrationImageConfig m_config;
        int m_width;
        int m_height;

        // 누적용 polarity별 event 개수 버퍼(CV_32SC1). Render()에서 우세도를 비교해 3-레벨 이미지로 변환.
        cv::Mat m_posCount;
        cv::Mat m_negCount;

        bool m_haveStart = false;  // 이번 누적 구간의 시작 시각을 이미 기록했는지
        lli m_startUs = 0;
        lli m_lastUs = 0;

        cv::Mat m_image;  // 마지막으로 완성된 CV_8UC1 이미지
    };
}
