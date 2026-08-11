// EventProcessing.Console.cpp
//
// Metavision EVK4 HD / IMX636 이벤트 카메라의 RAW 녹화(.raw), 실시간 카메라("live"),
// 또는 테스트용 CSV(.csv)를 읽어 event accumulation 이미지와 동영상으로 출력한다.

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "EventProcessor.h"
#include "EventSourceFactory.h"
#include "EventVisualizer.h"
#include "MetavisionRuntime.h"

using namespace eventcore;
namespace fs = std::filesystem;

namespace
{
    void PrintUsage(const char* exeName)
    {
        std::cout
            << "Usage: " << exeName << " <input.raw|input.csv|live> [outputDir] [windowUs] [fps]\n"
            << "  input.raw   Metavision EVK4 HD / IMX636 RAW recording\n"
            << "  input.csv   t_us,x,y,p CSV recording (for testing)\n"
            << "  live        connect to the first available EVK4 HD camera\n"
            << "  outputDir   output folder (default: output)\n"
            << "  windowUs    event accumulation window in microseconds (default: 10000)\n"
            << "  fps         output video frame rate (default: 30)\n";
    }
}

int main(int argc, char** argv)
{
#ifdef EVENTCORE_HAVE_METAVISION
    EnsureBundledHalPluginPath();
#endif

    if (argc < 2)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    const std::string input = argv[1];
    const std::string outputDir = argc > 2 ? argv[2] : "output";
    const lli windowUs = (argc > 3 && std::stoll(argv[3]) > 0) ? std::stoll(argv[3]) : 10000;
    const double fps = argc > 4 ? std::stod(argv[4]) : 30.0;

    std::unique_ptr<IEventSource> source = EventSourceFactory::CreateForPath(input);

    if (!source || !source->Open(input.c_str()))
    {
        std::cerr << "Failed to open event source: " << input << std::endl;
        return 1;
    }

    const int width = source->Width();
    const int height = source->Height();

    lli startAll = source->FirstTimestampUs();
    lli endAll = source->LastTimestampUs();

    if (endAll <= startAll)
    {
        endAll = startAll + windowUs;
    }

    std::error_code ec;
    fs::create_directories(outputDir, ec);

    EventVideoWriter videoWriter;
    const std::string videoPath = (fs::path(outputDir) / "event_video.mp4").string();

    if (!videoWriter.Open(videoPath, fps, cv::Size(width, height)))
    {
        std::cerr << "Failed to open video writer: " << videoPath << std::endl;
        return 1;
    }

    int windowIndex = 0;

    for (lli start = startAll; start < endAll; start += windowUs)
    {
        std::vector<Event> events;
        source->ReadEvents(events, start, start + windowUs);

        const EventProcessingResult result = EventProcessor::Process(events, width, height, start, windowUs);

        videoWriter.WriteFrame(result.debugImage);

        if (windowIndex == 0)
        {
            cv::imwrite((fs::path(outputDir) / "01_positive_event.png").string(), result.positiveImage);
            cv::imwrite((fs::path(outputDir) / "02_negative_event.png").string(), result.negativeImage);
            cv::imwrite((fs::path(outputDir) / "03_merged_event.png").string(), result.mergedImage);
            cv::imwrite((fs::path(outputDir) / "04_binary_mask.png").string(), result.binaryMask);
            cv::imwrite((fs::path(outputDir) / "05_debug_result.png").string(), result.debugImage);
        }

        ++windowIndex;
    }

    videoWriter.Close();

    std::cout
        << "Processed " << windowIndex << " event frame(s) from '" << input << "'.\n"
        << "Video : " << videoPath << "\n"
        << "Images: " << outputDir << "\n";

    return 0;
}
