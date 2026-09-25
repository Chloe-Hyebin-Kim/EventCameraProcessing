#pragma once

#include <string>

#include "CameraCalibrator.h"  // CalibrationResult

namespace eventcore
{
    // Calibration 결과를 파일로 저장/불러온다. OpenCV FileStorage(YAML/XML)를 사용한다
    // (확장자 .yml/.yaml -> YAML, .xml -> XML). OpenCV로 다시 읽어 undistort 등에 바로 쓸 수 있다.
    //
    // 저장 필드(요구사항):
    //   image_width, image_height
    //   camera_matrix: { fx, fy, cx, cy }
    //   distortion_coefficients: { k1, k2, p1, p2, k3 }
    //   rms_reprojection_error
    //   checkerboard: { rows, columns, square_size }
    //   number_of_observations
    //   model (참고용, 예: "pinhole")
    //
    // 경로 인코딩: OpenCV FileStorage는 좁은(narrow) 문자열 경로를 시스템 인코딩으로 해석한다.
    // 따라서 cv::imwrite와 동일하게, 호출부(GUI)에서 native 인코딩 문자열로 변환해 넘긴다.
    class CalibrationIO
    {
    public:
        // 성공 시 true. 실패하면 false를 반환하고 error(있으면)에 원인을 담는다.
        static bool Save(const std::string& nativePath, const CalibrationResult& result, std::string* error = nullptr);

        // 파일에서 읽어 result를 채운다(cameraMatrix/distCoeffs를 named 필드로부터 재구성).
        // 성공 시 result.success = true. per-view 오차/rvecs 등 저장되지 않는 항목은 비어 있다.
        static bool Load(const std::string& nativePath, CalibrationResult& result, std::string* error = nullptr);
    };
}
