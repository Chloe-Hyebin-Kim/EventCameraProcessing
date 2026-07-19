#include "pch.h"
#include "EventAccumulator.h"

namespace eventcore
{
    cv::Mat EventAccumulator::NormalizeTo8U(const cv::Mat& src)
    {
        double minVal = 0.0;
        double maxVal = 0.0;

        cv::minMaxLoc(src, &minVal, &maxVal);

        if (maxVal <= 0.0)
        {
            return cv::Mat::zeros(src.size(), CV_8UC1);
        }

        cv::Mat dst;
        src.convertTo(dst, CV_8UC1, 255.0 / maxVal);

        return dst;
    }

    void EventAccumulator::Accumulate(
        const std::vector<Event>& events,
        int width,
        int height,
        int64_t startUs,
        int64_t windowUs,
        cv::Mat& positiveImage,
        cv::Mat& negativeImage,
        cv::Mat& mergedImage)
    {
        const int64_t endUs = startUs + windowUs;

        cv::Mat posAccum = cv::Mat::zeros(height, width, CV_32SC1);
        cv::Mat negAccum = cv::Mat::zeros(height, width, CV_32SC1);

        for (const auto& e : events)
        {
            if (e.t_us < startUs || e.t_us >= endUs)
            {
                continue;
            }

            if (e.x < 0 || e.x >= width || e.y < 0 || e.y >= height)
            {
                continue;
            }

            if (e.polarity > 0)
            {
                posAccum.at<int>(e.y, e.x) += 1;
            }
            else
            {
                negAccum.at<int>(e.y, e.x) += 1;
            }
        }

        positiveImage = NormalizeTo8U(posAccum);
        negativeImage = NormalizeTo8U(negAccum);

        cv::Mat mergedAccum = posAccum + negAccum;
        mergedImage = NormalizeTo8U(mergedAccum);
    }
}