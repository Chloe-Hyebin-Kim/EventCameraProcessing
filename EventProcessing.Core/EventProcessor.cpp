#include "pch.h"
#include "EventProcessor.h"

#include "EventAccumulator.h"
#include "EventFilter.h"

namespace eventcore
{
    EventProcessingResult EventProcessor::Process(
        const std::vector<Event>& events,
        int width,
        int height,
        int64_t startUs,
        int64_t windowUs)
    {
        EventProcessingResult result;

        EventAccumulator::Accumulate(
            events,
            width,
            height,
            startUs,
            windowUs,
            result.positiveImage,
            result.negativeImage,
            result.mergedImage
        );

        result.binaryMask = EventFilter::RemoveSmallNoise(result.mergedImage);

        result.ball = BallDetector::Detect(result.binaryMask);

        cv::cvtColor(result.mergedImage, result.debugImage, cv::COLOR_GRAY2BGR);

        if (result.ball.detected)
        {
            cv::circle(
                result.debugImage,
                result.ball.center,
                static_cast<int>(result.ball.radius),
                cv::Scalar(0, 255, 0),
                2
            );

            cv::circle(
                result.debugImage,
                result.ball.center,
                3,
                cv::Scalar(0, 0, 255),
                -1
            );

            cv::rectangle(
                result.debugImage,
                result.ball.boundingBox,
                cv::Scalar(255, 0, 0),
                2
            );
        }

        return result;
    }
}