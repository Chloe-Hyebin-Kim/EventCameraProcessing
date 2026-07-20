#include "pch.h"
#include "EventFilter.h"

namespace eventcore
{
    cv::Mat EventFilter::RemoveSmallNoise(const cv::Mat& input)
    {
        cv::Mat blurred;
        cv::GaussianBlur(input, blurred, cv::Size(3, 3), 0.0);

        cv::Mat binary;
        cv::threshold(blurred, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3) );

        cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);

        return binary;
    }
}