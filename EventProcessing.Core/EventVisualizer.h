#pragma once

#include <string>
#include <opencv2/opencv.hpp>

namespace eventcore
{
    class EventVisualizer
    {
    public:
        // positiveImage/negativeImage: 8U single-channel accumulation images (see EventAccumulator).
        // Returns a BGR frame: black background, positive(ON) events green, negative(OFF) events red.
        static cv::Mat ToColorFrame(const cv::Mat& positiveImage, const cv::Mat& negativeImage);
    };

    // Streams accumulated event frames to an .mp4/.avi file so a RAW recording can be watched as video.
    class EventVideoWriter
    {
    public:
        bool Open(const std::string& outputPath, double fps, cv::Size frameSize);
        void WriteFrame(const cv::Mat& bgrFrame);
        void Close();

        bool IsOpened() const;

    private:
        cv::VideoWriter m_writer;
    };
}
