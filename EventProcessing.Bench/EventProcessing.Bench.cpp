// EventProcessing.Bench
//
// 녹화된 .raw / .csv를 ShotRecorder 파이프라인으로 오프라인 처리해
// 측정값 CSV와 샷별 raw 이벤트 덤프를 만든다.
//
// 왜 별도 도구인가:
//   - GUI를 건드리지 않고도 새 파이프라인(강건 검출 + 서브픽셀 적합 + 프리트리거 덤프)을
//     이미 녹화해 둔 파일에 바로 돌려볼 수 있다.
//   - 윈도우 길이를 마음대로 바꿔가며 같은 녹화를 재처리할 수 있다.
//     Phase 0에서 "몇 us 윈도우면 신호가 충분한가"를 답하려면 이게 필요하다.
//   - 윈도우 경계를 벽시계가 아니라 이벤트 타임스탬프로 자른다.
//     (LiveEventStream은 sleep_for로 자르는데, Windows 스케줄러 지터가 1~15 ms다.
//      108 m/s에서 1 ms = 10.8 cm이므로 측정용으로는 쓸 수 없다.)
//
// 사용법:
//   EventProcessing.Bench <input.raw|input.csv> [옵션]
//
//   --out <dir>            출력 폴더 (기본: bench_out)
//   --window-us <n>        누적 윈도우 길이 (기본: 1000)
//   --from-us <n>          이 시각부터 처리 (기본: 파일 처음)
//   --to-us <n>            이 시각까지 처리 (기본: 파일 끝)
//   --pre-us <n>           트리거 이전 덤프 길이 (기본: 30000)
//   --post-us <n>          트리거 이후 덤프 길이 (기본: 30000)
//   --ring-us <n>          링버퍼 보관 길이 (기본: 300000)
//   --ring-max <n>         링버퍼 이벤트 상한 (기본: 12000000, 약 288 MB)
//   --min-radius <px>      공 반지름 하한 (기본: 3)
//   --max-radius <px>      공 반지름 상한 (기본: 0 = 제한 없음. 표준거리가 정해지면 꼭 설정할 것)
//   --min-circularity <v>  원형도 하한 (기본: 0.55)
//   --ready-sec <v>        레디 판정 시간 (기본: 1.0)
//   --shot-speed <px/s>    샷 판정 속도 (기본: 400)
//   --no-dump              이벤트 덤프를 만들지 않음(측정 CSV만)
//   --frames <dir>         윈도우별 debug 이미지를 PNG로 저장(느림. 소량 구간에만 쓸 것)
//
// 예:
//   EventProcessing.Bench shot.raw --out bench_1ms  --window-us 1000
//   EventProcessing.Bench shot.raw --out bench_200us --window-us 200 --max-radius 40
//   python scripts/analyze_shot.py bench_200us --shot 1 --window-us 200

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "EventProcessor.h"
#include "EventSourceFactory.h"
#include "ShotRecorder.h"
#include "Utf8Path.h"

#ifdef EVENTCORE_HAVE_METAVISION
#include "MetavisionRuntime.h"
#endif

using namespace eventcore;
namespace fs = std::filesystem;

namespace
{
    struct Options
    {
        std::string input;
        std::string outputDir = "bench_out";
        std::string framesDir;

        lli windowUs = 1000;
        lli fromUs = -1;
        lli toUs = -1;

        lli preUs = 30000;
        lli postUs = 30000;
        lli ringUs = 300000;
        std::size_t ringMax = 12000000;

        float minRadius = 3.0f;
        float maxRadius = 0.0f;
        double minCircularity = 0.55;

        double readySec = 1.0;
        float shotSpeed = 400.0f;

        bool dump = true;
        bool eventCloud = true;
        int cellPx = 8;
    };

    void PrintUsage()
    {
        std::cout <<
            "Usage: EventProcessing.Bench <input.raw|input.csv> [options]\n"
            "  --out <dir>            output folder (default: bench_out)\n"
            "  --window-us <n>        accumulation window (default: 1000)\n"
            "  --from-us <n>          start time (default: file start)\n"
            "  --to-us <n>            end time (default: file end)\n"
            "  --pre-us <n>           dump length before trigger (default: 30000)\n"
            "  --post-us <n>          dump length after trigger  (default: 30000)\n"
            "  --ring-us <n>          ring buffer span (default: 300000)\n"
            "  --ring-max <n>         ring buffer event cap (default: 12000000)\n"
            "  --min-radius <px>      minimum ball radius (default: 3)\n"
            "  --max-radius <px>      maximum ball radius (default: 0 = unlimited)\n"
            "  --min-circularity <v>  minimum circularity (default: 0.55)\n"
            "  --ready-sec <v>        seconds of stillness for Ready (default: 1.0)\n"
            "  --shot-speed <px/s>    centre speed that fires the trigger (default: 400)\n"
            "  --no-dump              measurements only, no event dumps\n"
            "  --mask-path            use the binary-mask/contour detector instead of the\n"
            "                         event point cloud (only sensible at long windows)\n"
            "  --cell-px <n>          density grid cell size for the cloud path (default: 8)\n"
            "  --frames <dir>         write per-window debug PNGs (slow)\n";
    }

    bool ParseArgs(int argc, char** argv, Options& o)
    {
        if (argc < 2)
        {
            return false;
        }

        o.input = argv[1];

        for (int i = 2; i < argc; ++i)
        {
            const std::string a = argv[i];
            auto next = [&](const char* name) -> std::string
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "missing value for " << name << "\n";
                    std::exit(1);
                }
                return argv[++i];
            };

            if      (a == "--out")             o.outputDir = next("--out");
            else if (a == "--frames")          o.framesDir = next("--frames");
            else if (a == "--window-us")       o.windowUs = std::stoll(next("--window-us"));
            else if (a == "--from-us")         o.fromUs = std::stoll(next("--from-us"));
            else if (a == "--to-us")           o.toUs = std::stoll(next("--to-us"));
            else if (a == "--pre-us")          o.preUs = std::stoll(next("--pre-us"));
            else if (a == "--post-us")         o.postUs = std::stoll(next("--post-us"));
            else if (a == "--ring-us")         o.ringUs = std::stoll(next("--ring-us"));
            else if (a == "--ring-max")        o.ringMax = static_cast<std::size_t>(std::stoll(next("--ring-max")));
            else if (a == "--min-radius")      o.minRadius = std::stof(next("--min-radius"));
            else if (a == "--max-radius")      o.maxRadius = std::stof(next("--max-radius"));
            else if (a == "--min-circularity") o.minCircularity = std::stod(next("--min-circularity"));
            else if (a == "--ready-sec")       o.readySec = std::stod(next("--ready-sec"));
            else if (a == "--shot-speed")      o.shotSpeed = std::stof(next("--shot-speed"));
            else if (a == "--no-dump")         o.dump = false;
            else if (a == "--mask-path")       o.eventCloud = false;
            else if (a == "--cell-px")         o.cellPx = std::stoi(next("--cell-px"));
            else if (a == "-h" || a == "--help") return false;
            else
            {
                std::cerr << "unknown option: " << a << "\n";
                return false;
            }
        }

        if (o.windowUs <= 0)
        {
            std::cerr << "--window-us must be > 0\n";
            return false;
        }

        return true;
    }
}

int main(int argc, char** argv)
{
#ifdef EVENTCORE_HAVE_METAVISION
    EnsureBundledHalPluginPath();
#endif

    Options opt;

    if (!ParseArgs(argc, argv, opt))
    {
        PrintUsage();
        return 1;
    }

    std::unique_ptr<IEventSource> source = EventSourceFactory::CreateForPath(opt.input);

    if (!source || !source->Open(opt.input.c_str()))
    {
        std::cerr << "Failed to open event source: " << opt.input << "\n";
        return 1;
    }

    const int width = source->Width();
    const int height = source->Height();

    const lli fileFirst = source->FirstTimestampUs();
    const lli fileLast = source->LastTimestampUs();

    const lli startUs = (opt.fromUs >= 0) ? std::max(opt.fromUs, fileFirst) : fileFirst;
    const lli endUs = (opt.toUs >= 0) ? std::min(opt.toUs, fileLast + 1) : (fileLast + 1);

    if (endUs <= startUs)
    {
        std::cerr << "empty time range: [" << startUs << ", " << endUs << ")\n";
        return 1;
    }

    // 전체 구간을 한 번에 읽는다.
    //
    // 기존 Console 앱처럼 윈도우마다 ReadEvents()를 부르면, CsvEventSource와
    // MetavisionEventSource 모두 매번 전체 이벤트를 선형 스캔하므로
    // 비용이 O(윈도우 수 x 이벤트 수)가 된다. 윈도우를 10 ms에서 200 us로 줄이면
    // 윈도우 수가 50배가 되어 그대로 터진다. 여기서는 한 번만 읽고 커서로 자른다.
    std::vector<Event> all;

    if (!source->ReadEvents(all, startUs, endUs))
    {
        std::cerr << "ReadEvents failed\n";
        return 1;
    }

    if (all.empty())
    {
        std::cerr << "no events in [" << startUs << ", " << endUs << ")\n";
        return 1;
    }

    // 커서 방식으로 자르려면 시간순 정렬이 보장되어야 한다.
    // 파일에 따라 완전히 단조롭지 않을 수 있으므로 확인하고, 필요하면 안정 정렬한다.
    if (!std::is_sorted(all.begin(), all.end(),
                        [](const Event& a, const Event& b) { return a.t_us < b.t_us; }))
    {
        std::cout << "events are not time-ordered; sorting " << all.size() << " events...\n";
        std::stable_sort(all.begin(), all.end(),
                         [](const Event& a, const Event& b) { return a.t_us < b.t_us; });
    }

    std::cout << "input        : " << opt.input << "\n"
              << "sensor       : " << width << " x " << height << "\n"
              << "time range   : " << startUs << " .. " << endUs << " us ("
              << (endUs - startUs) / 1000.0 << " ms)\n"
              << "events       : " << all.size() << "\n"
              << "window       : " << opt.windowUs << " us\n"
              << "detector     : " << (opt.eventCloud ? "event point cloud" : "binary mask contours") << "\n";

    // 이 윈도우 길이에서 공이 얼마나 도는지 미리 보여준다(로드맵 04절).
    {
        const double drvDeg = 2500.0 / 60.0 * (opt.windowUs * 1e-6) * 360.0;
        const double wdgDeg = 10000.0 / 60.0 * (opt.windowUs * 1e-6) * 360.0;
        std::cout << std::fixed << std::setprecision(2)
                  << "rotation/win : driver 2500 rpm -> " << drvDeg << " deg, "
                  << "wedge 10000 rpm -> " << wdgDeg << " deg\n";
    }

    ShotRecorderConfig cfg;
    cfg.outputDir = opt.outputDir;
    cfg.ring.spanUs = opt.ringUs;
    cfg.ring.maxEvents = opt.ringMax;
    cfg.preTriggerUs = opt.preUs;
    cfg.postTriggerUs = opt.postUs;
    cfg.dumpEvents = opt.dump;
    cfg.logMeasurements = true;
    cfg.trigger.readySeconds = opt.readySec;
    cfg.trigger.shotSpeedPxPerSec = opt.shotSpeed;
    cfg.gate.minRadiusPx = opt.minRadius;
    cfg.gate.maxRadiusPx = opt.maxRadius;
    cfg.gate.minCircularity = opt.minCircularity;
    cfg.useEventCloud = opt.eventCloud;
    cfg.cloud.cellPx = opt.cellPx;

    ShotRecorder recorder;

    if (!recorder.Begin(cfg))
    {
        std::cerr << "ShotRecorder::Begin failed\n";
        return 1;
    }

    if (!opt.framesDir.empty())
    {
        std::error_code ec;
        fs::create_directories(Utf8ToPath(opt.framesDir), ec);
    }

    std::size_t cursor = 0;
    long long windows = 0;
    long long detections = 0;
    long long trusted = 0;
    long long dumps = 0;
    bool warnedOverflow = false;

    std::vector<Event> windowEvents;
    windowEvents.reserve(1 << 16);

    for (lli wStart = startUs; wStart < endUs; wStart += opt.windowUs)
    {
        const lli wEnd = wStart + opt.windowUs;

        windowEvents.clear();

        while (cursor < all.size() && all[cursor].t_us < wEnd)
        {
            if (all[cursor].t_us >= wStart)
            {
                windowEvents.push_back(all[cursor]);
            }
            ++cursor;
        }

        const EventProcessingResult processed =
            EventProcessor::Process(windowEvents, width, height, wStart, opt.windowUs);

        const ShotRecorderUpdate update =
            recorder.OnWindow(windowEvents, processed.binaryMask, wStart, wEnd);

        ++windows;

        if (update.detected)
        {
            ++detections;

            if (update.radiusTrusted)
            {
                ++trusted;
            }
        }

        if (update.ringOverflowed && !warnedOverflow)
        {
            warnedOverflow = true;
            std::cout << "[warn] ring buffer hit its event cap; raise --ring-max "
                         "or lower the event rate (ROI / bias)\n";
        }

        if (update.dumpWritten)
        {
            ++dumps;
            std::cout << "[shot] dumped " << update.dumpEventCount << " events to "
                      << update.dumpPath << "  (" << update.dumpFromUs << " .. "
                      << update.dumpToUs << " us)\n";
        }

        if (!opt.framesDir.empty() && !processed.debugImage.empty())
        {
            std::ostringstream name;
            name << "w" << std::setfill('0') << std::setw(8) << windows << ".png";
            cv::imwrite((fs::path(Utf8ToPath(opt.framesDir)) / name.str()).string(), processed.debugImage);
        }
    }

    recorder.End();

    // End()가 마지막에 강제 플러시한 샷도 포함해야 하므로 recorder에게 직접 묻는다.
    dumps = recorder.ShotCount();

    std::cout << "\nwindows      : " << windows << "\n"
              << "detections   : " << detections
              << " (" << (windows ? 100.0 * detections / windows : 0.0) << " %)\n"
              << "radius trusted: " << trusted
              << " (" << (windows ? 100.0 * trusted / windows : 0.0) << " %)\n"
              << "shots dumped : " << dumps << "\n"
              << "measurements : " << recorder.MeasurementCsvPath() << "\n"
              << "\nnext: python scripts/analyze_shot.py " << opt.outputDir
              << " --window-us " << opt.windowUs << "\n";

    return 0;
}
