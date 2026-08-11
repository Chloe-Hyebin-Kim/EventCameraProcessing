#include "pch.h"
#include "EventVisualizer.h"

namespace eventcore
{
    cv::Mat EventVisualizer::ToColorFrame(const cv::Mat& positiveImage, const cv::Mat& negativeImage)
    {
        const cv::Size size = positiveImage.empty() ? negativeImage.size() : positiveImage.size();

        cv::Mat zero = cv::Mat::zeros(size, CV_8UC1);
        const cv::Mat& pos = positiveImage.empty() ? zero : positiveImage;
        const cv::Mat& neg = negativeImage.empty() ? zero : negativeImage;

        std::vector<cv::Mat> channels = { zero, pos, neg }; // B, G(=positive), R(=negative)

        cv::Mat frame;
        cv::merge(channels, frame);

        return frame;
    }

    bool EventVideoWriter::Open(const std::string& outputPath, double fps, cv::Size frameSize)
    {
        const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        return m_writer.open(outputPath, fourcc, fps, frameSize, true);
    }

    void EventVideoWriter::WriteFrame(const cv::Mat& bgrFrame)
    {
        if (m_writer.isOpened())
        {
            m_writer.write(bgrFrame);
        }
    }

    void EventVideoWriter::Close()
    {
        if (m_writer.isOpened())
        {
            m_writer.release();
        }
    }

    bool EventVideoWriter::IsOpened() const
    {
        return m_writer.isOpened();
    }
}
