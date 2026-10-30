#pragma once

#include <opencv2/opencv.hpp>

namespace eventcore
{
    // 움직이는(인쇄) 체커보드에서 얻은 event 누적 결과로부터 "칸이 채워진" 체커보드 이미지를 재구성한 결과.
    struct EdgeReconstruction
    {
        bool ok = false;
        // CV_8UC1. 한 색의 칸 = 255, 반대 색의 칸 = 0, 판단 불가/배경 = 127.
        // 흑백이 실제와 뒤바뀌어 있을 수 있다(OpenCV 체커보드 검출은 반전에 무관).
        cv::Mat image;
        int cellCount = 0;      // 색이 정해진(가장 큰 연결 그룹의) 칸 수
        int conflictCount = 0;  // 2색 조건을 어긴 이웃 관계 수(0이 이상적; 디버깅/품질 지표)
    };

    // 문제: event camera는 밝기 변화가 있는 곳에서만 event를 낸다. 체커보드를 움직이면 칸의 경계(edge)에만
    //       event가 생기고 칸 내부는 비어 있어, polarity 누적 이미지가 "윤곽선"이 된다.
    //       findChessboardCorners/SB는 채워진 흑백 칸을 전제로 하므로 이 이미지에서는 검출되지 않는다.
    //
    // 방법(OpenCV core/imgproc만 사용):
    //  1) event 활동 마스크: 픽셀별 event 유무의 5x5 국소 밀도를, 이미지 전체에서 추정한 노이즈 밀도
    //     (중앙값) 기준으로 이진화한다 -> 노이즈 점은 지우고 경계 띠만 남긴다. 이어서 closing으로 작은 끊김을 메운다.
    //  2) 경계가 아닌 영역의 connected component = 칸 후보. 면적이 비슷한 것들을 칸으로 본다.
    //  3) 경계 띠 픽셀을 가장 가까운 영역에 배정(Voronoi, distanceTransform label)해, 영역 쌍마다 공유 경계 길이를
    //     잰다. 변으로 맞닿은 칸(공유 경계가 김)은 서로 다른 색, 대각선 칸(한 점에서만 만남)은 이웃이 아니다.
    //  4) 공유 경계가 긴 순서로 union-find(패리티)를 돌려 2색을 칠한다. 체커보드 바깥 흰 여백처럼 칸이 아닌 큰
    //     영역도 칸과 변으로 맞닿아 있으면 같은 그래프에서 색이 정해진다(바깥 줄 흰 칸이 여백과 합쳐지는 경우 대응).
    //  5) 경계 띠는 배정된 영역의 색으로 채워 경계선 없는 체커보드 이미지를 만든다. 코너는 띠의 중앙선,
    //     즉 누적 시간 중간 무렵의 경계 위치에 놓인다.
    //
    // 한계(시뮬레이션으로 확인, 실기기 미검증):
    //  - 보드를 한 축 방향으로만 움직이면 그 방향과 평행한 경계는 event를 내지 않아 칸이 닫히지 않는다 -> 실패.
    //    대각선 방향(가로+세로 성분)으로 움직여야 한다.
    //  - 회전이 커서 보드 일부의 국소 움직임이 0에 가까우면 그 부분 칸이 합쳐져 실패할 수 있다.
    //  - 누적 시간 동안 이동 거리가 칸 크기에 가까워지면 칸이 사라져 실패한다.
    class EdgeCheckerboardReconstructor
    {
    public:
        // eventCount: 누적 구간 동안 픽셀별 event 개수(극성 무관). CV_8UC1/CV_16UC1/CV_32SC1 등 단일 채널.
        static EdgeReconstruction Reconstruct(const cv::Mat& eventCount);
    };
}
