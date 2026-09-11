#include "pch.h"
#include "ShotRecorder.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <system_error>

#include "Utf8Path.h"

namespace fs = std::filesystem;

namespace eventcore
{
    namespace
    {
        std::string ShotFolderName(int index)
        {
            std::ostringstream oss;
            oss << "shot_" << std::setfill('0') << std::setw(4) << index;
            return oss.str();
        }

        double Distance(const cv::Point2f& a, const cv::Point2f& b)
        {
            const double dx = static_cast<double>(a.x) - static_cast<double>(b.x);
            const double dy = static_cast<double>(a.y) - static_cast<double>(b.y);
            return std::sqrt(dx * dx + dy * dy);
        }
    }

    ShotRecorder::ShotRecorder() = default;

    ShotRecorder::~ShotRecorder()
    {
        End();
    }

    bool ShotRecorder::Begin(const ShotRecorderConfig& config)
    {
        End();

        m_config = config;

        m_ring.Configure(m_config.ring);

        ShotTriggerConfig triggerConfig = m_config.trigger;

        if (m_config.useDepartureTrigger)
        {
            // 이탈 트리거를 쓸 때 ShotTrigger는 "Ready 판정"에만 쓴다.
            // ShotTrigger 자체의 속도 기반 트리거는 반드시 꺼야 한다.
            //
            // 이유(합성 데이터에서 재현): 한 윈도우 검출을 놓쳤다가 다른 위치에서 다시 잡히면
            // 겉보기 중심 이동속도가 순간적으로 치솟아 헛트리거가 난다. 그때마다 Impact로
            // 갔다가 취소/리셋되면서 Ready 누적이 초기화되고, 결국 진짜 샷 시점에 Ready가
            // 아니어서 이탈 트리거도 걸리지 않는다. 실제로 2 ms 윈도우에서 전체 608 윈도우 중
            // Ready가 18개뿐이었고 진짜 샷을 놓쳤다.
            triggerConfig.shotSpeedPxPerSec = std::numeric_limits<float>::max();
        }

        m_trigger = ShotTrigger(triggerConfig);
        m_trigger.Reset();

        m_shotIndex = 0;
        m_dumpPending = false;
        m_pendingTriggerUs = 0;
        m_haveTrack = false;
        m_havePrevCenter = false;
        m_pendingCancelled = false;
        m_havePendingAnchor = false;
        m_haveStableCenter = false;
        m_baseline.clear();
        m_baselineNext = 0;
        m_lastTriggerUs = -1;

        std::error_code ec;
        fs::create_directories(Utf8ToPath(m_config.outputDir), ec);

        if (ec)
        {
            std::cerr << "ShotRecorder::Begin: failed to create " << m_config.outputDir
                      << " (" << ec.message() << ")" << std::endl;
            return false;
        }

        if (m_config.logMeasurements)
        {
            const std::string csv = (fs::path(Utf8ToPath(m_config.outputDir)) / "measurements.csv").string();

            if (!m_logger.Open(csv))
            {
                return false;
            }
        }

        m_active = true;
        return true;
    }

    ShotRecorderUpdate ShotRecorder::OnWindow(const std::vector<Event>& windowEvents,
                                              const cv::Mat& binaryMask,
                                              lli windowStartUs,
                                              lli windowEndUs)
    {
        ShotRecorderUpdate update;

        if (!m_active)
        {
            return update;
        }

        if (!binaryMask.empty())
        {
            m_width = binaryMask.cols;
            m_height = binaryMask.rows;
        }

        // 1) 항상 먼저 쌓는다. 트리거 여부와 무관하게 히스토리가 남아 있어야 한다.
        const std::size_t droppedBefore = m_ring.DroppedByCapacity();
        m_ring.Push(windowEvents);
        update.ringOverflowed = (m_ring.DroppedByCapacity() > droppedBefore);

        // 2) 공 검출. 직전 위치가 아직 유효하면 트래킹 힌트로 쓴다.
        BallGate gate = m_config.gate;

        if (m_config.useTracking && m_haveTrack &&
            (windowEndUs - m_trackUs) <= m_config.trackTimeoutUs)
        {
            gate.useTrackHint = true;
            gate.trackHint = m_trackHint;
            gate.trackGatePx = m_config.trackGatePx;
        }
        else
        {
            gate.useTrackHint = false;
            gate.trackGatePx = 0.0f;
        }

        if (m_config.useEventCloud)
        {
            // raw 이벤트 점군에 직접 원 적합. 짧은 윈도우에서 유일하게 성립하는 경로.
            update.cloudBall = EventBallFinder::Find(windowEvents, m_width, m_height,
                                                     gate, m_config.cloud, m_config.fit);

            update.ball = update.cloudBall.ToLegacy();
            update.detected = update.cloudBall.detected;
            update.center = update.cloudBall.center;
            update.radius = update.cloudBall.radius;
            update.radiusTrusted = update.cloudBall.radiusTrusted;
            update.fit = update.cloudBall.fit;
            update.reject = update.cloudBall.reject;
        }
        else
        {
            update.maskBall = RobustBallDetector::Detect(binaryMask, gate, m_config.fit);

            update.ball = update.maskBall.ToLegacy();
            update.detected = update.maskBall.detected;
            update.center = update.maskBall.center;
            update.radius = update.maskBall.radius;
            update.radiusTrusted = update.maskBall.radiusTrusted;
            update.fit = update.maskBall.fit;
            update.reject = update.maskBall.reject;
        }

        if (update.detected)
        {
            m_haveTrack = true;
            m_trackHint = update.center;
            m_trackUs = windowEndUs;
        }

        // 트리거 직전의 "정지해 있던 위치". 헛트리거 판정의 기준점이 된다.
        // Impact/Trajectory 중이 아닐 때의 위치만 "어드레스 위치"로 본다.
        if (update.detected &&
            m_trigger.State() != ShotState::Impact &&
            m_trigger.State() != ShotState::Trajectory)
        {
            m_haveStableCenter = true;
            m_stableCenter = update.center;
        }

        // 3) ShotTrigger 갱신. 상태 판단의 시각은 이벤트 타임스탬프 기준(윈도우 끝)을 쓴다.
        //    벽시계를 쓰면 안 된다 - 108 m/s에서 1 ms = 10.8 cm 이다.
        update.shot = m_trigger.Update(update.ball, windowEndUs);

        // 3b) 이탈 트리거. ShotTrigger가 Ready를 잡아 준 상태에서만 동작한다.
        if (m_config.useDepartureTrigger &&
            !m_dumpPending &&
            update.shot.state == ShotState::Ready &&
            m_haveStableCenter &&
            !m_baseline.empty())
        {
            std::vector<int> sorted = m_baseline;
            std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
            const double baseline = static_cast<double>(sorted[sorted.size() / 2]);

            const bool burst =
                static_cast<double>(windowEvents.size()) >= std::max(baseline * m_config.burstFactor,
                                                                     static_cast<double>(m_config.minBurstEvents));

            const bool gone =
                !update.detected ||
                Distance(update.center, m_stableCenter) > static_cast<double>(m_config.departurePx);

            const bool refractory =
                (m_lastTriggerUs >= 0) && ((windowEndUs - m_lastTriggerUs) < m_config.refractoryUs);

            if (burst && gone && !refractory)
            {
                update.shot.justTriggered = true;
                update.shot.state = ShotState::Impact;

                m_dumpPending = true;
                m_pendingCancelled = false;
                m_pendingTriggerUs = windowEndUs;
                m_havePendingAnchor = true;
                m_pendingAnchor = m_stableCenter;
                m_lastTriggerUs = windowEndUs;

                // 샷이 나갔으면 새 공을 올려 다시 Ready가 될 때까지 기다려야 한다.
                // 리셋하지 않으면 ShotTrigger가 계속 Ready로 남아 다음 윈도우에서
                // 같은 샷이 또 트리거된다(실제로 재현된 버그).
                m_trigger.Reset();
                m_haveStableCenter = false;
                m_haveTrack = false;
            }
        }

        // 기준선 갱신은 판정 뒤에 한다(폭증 윈도우가 자기 기준선을 오염시키지 않도록).
        {
            const int n = std::max(1, m_config.baselineWindows);

            if (static_cast<int>(m_baseline.size()) < n)
            {
                m_baseline.push_back(static_cast<int>(windowEvents.size()));
            }
            else
            {
                m_baseline[m_baselineNext % m_baseline.size()] = static_cast<int>(windowEvents.size());
                ++m_baselineNext;
            }
        }

        // 4) 중심 이동속도(진단용)
        double speedPxPerSec = -1.0;

        if (update.detected)
        {
            if (m_havePrevCenter && windowEndUs > m_prevCenterUs)
            {
                const double dt = static_cast<double>(windowEndUs - m_prevCenterUs) / 1000000.0;
                speedPxPerSec = Distance(update.center, m_prevCenter) / dt;
            }

            m_havePrevCenter = true;
            m_prevCenter = update.center;
            m_prevCenterUs = windowEndUs;
        }

        // 5) 측정값 기록
        if (m_config.logMeasurements && m_logger.IsOpen())
        {
            ShotMeasurement meas;

            meas.windowStartUs = windowStartUs;
            meas.windowEndUs = windowEndUs;
            meas.eventCount = static_cast<int>(windowEvents.size());

            for (const Event& e : windowEvents)
            {
                if (e.polarity > 0)
                {
                    ++meas.onCount;
                }
                else
                {
                    ++meas.offCount;
                }
            }

            meas.detected = update.detected;
            meas.centerX = update.center.x;
            meas.centerY = update.center.y;
            meas.radiusPx = update.radius;
            meas.fitRmsPx = update.fit.rmsPx;
            meas.fitCoverage = update.fit.coverage;
            meas.radiusTrusted = update.radiusTrusted;
            meas.fitPointsUsed = update.fit.pointsUsed;
            meas.fitPointsTotal = update.fit.pointsTotal;
            meas.rejectReason = RobustBallDetector::RejectReasonName(update.reject);

            if (m_config.useEventCloud)
            {
                meas.momentCentroidX = update.cloudBall.eventCentroid.x;
                meas.momentCentroidY = update.cloudBall.eventCentroid.y;
                meas.areaPx = update.cloudBall.eventsUsed;      // 점군에서는 적합에 쓴 이벤트 수
                meas.circularity = update.cloudBall.fitRmsRatio; // 점군에서는 잔차비(작을수록 링에 가깝다)
                meas.aspectRatio = update.cloudBall.aspectRatio;
                meas.contoursTotal = update.cloudBall.clustersTotal;
                meas.contoursPassed = update.cloudBall.clustersPassed;
            }
            else
            {
                meas.momentCentroidX = update.maskBall.momentCentroid.x;
                meas.momentCentroidY = update.maskBall.momentCentroid.y;
                meas.areaPx = update.maskBall.area;
                meas.circularity = update.maskBall.circularity;
                meas.aspectRatio = update.maskBall.aspectRatio;
                meas.contoursTotal = update.maskBall.contoursTotal;
                meas.contoursPassed = update.maskBall.contoursPassed;
            }
            meas.shotState = static_cast<int>(update.shot.state);
            meas.justEnteredReady = update.shot.justEnteredReady;
            meas.justTriggered = update.shot.justTriggered;
            meas.centerSpeedPxPerSec = speedPxPerSec;

            m_logger.Append(meas);
        }

        // 6) 트리거 처리. 지금 바로 덤프하면 트리거 이후 구간이 아직 안 들어와 있으므로,
        //    postTriggerUs 만큼 더 쌓일 때까지 기다렸다가 되감아 저장한다.
        if (update.shot.justTriggered && !m_dumpPending && m_config.dumpEvents)
        {
            m_dumpPending = true;
            m_pendingCancelled = false;
            m_pendingTriggerUs = m_trigger.TriggerTimeUs();

            if (m_pendingTriggerUs < 0)
            {
                m_pendingTriggerUs = windowEndUs;
            }

            m_havePendingAnchor = m_haveStableCenter;
            m_pendingAnchor = m_stableCenter;
        }
        else if (m_dumpPending && m_config.triggerConfirm && m_havePendingAnchor &&
                 update.detected && !m_pendingCancelled)
        {
            // 트리거 직전 정지 위치 근처에서 공이 다시 보였다 -> 실제로는 나가지 않았다.
            if (Distance(update.center, m_pendingAnchor) <= static_cast<double>(m_config.confirmReturnPx))
            {
                m_pendingCancelled = true;
            }
        }

        if (m_dumpPending)
        {
            const bool ready = (m_ring.NewestUs() - m_pendingTriggerUs) >= m_config.postTriggerUs;

            if (ready && m_pendingCancelled)
            {
                update.triggerCancelled = true;
                m_dumpPending = false;
                m_havePendingAnchor = false;
                m_trigger.Reset();
            }
            else
            {
                FlushPendingDump(update, ready);
            }
        }

        return update;
    }

    bool ShotRecorder::FlushPendingDump(ShotRecorderUpdate& update, bool force)
    {
        if (!m_dumpPending || !force)
        {
            return false;
        }

        const lli fromUs = m_pendingTriggerUs - m_config.preTriggerUs;
        const lli toUs = m_pendingTriggerUs + m_config.postTriggerUs;

        std::vector<Event> dump;
        m_ring.CopyRange(dump, fromUs, toUs);

        ++m_shotIndex;

        const fs::path shotDir = fs::path(Utf8ToPath(m_config.outputDir)) / ShotFolderName(m_shotIndex);

        std::error_code ec;
        fs::create_directories(shotDir, ec);

        if (ec)
        {
            std::cerr << "ShotRecorder: failed to create " << shotDir.string()
                      << " (" << ec.message() << ")" << std::endl;
            m_dumpPending = false;
            return false;
        }

        const std::string csvPath = (shotDir / "events.csv").string();

        const bool ok = EventRingBuffer::WriteCsv(csvPath, dump);

        // 이 샷이 어떤 조건에서 잘렸는지 나중에 알 수 있도록 메타데이터도 같이 남긴다.
        {
            std::ofstream meta(shotDir / "shot_meta.txt", std::ios::binary);

            if (meta.is_open())
            {
                meta << "trigger_us=" << m_pendingTriggerUs << "\n"
                     << "dump_from_us=" << fromUs << "\n"
                     << "dump_to_us=" << toUs << "\n"
                     << "pre_trigger_us=" << m_config.preTriggerUs << "\n"
                     << "post_trigger_us=" << m_config.postTriggerUs << "\n"
                     << "event_count=" << dump.size() << "\n"
                     << "ring_oldest_us=" << m_ring.OldestUs() << "\n"
                     << "ring_newest_us=" << m_ring.NewestUs() << "\n"
                     << "ring_size=" << m_ring.Size() << "\n"
                     << "ring_dropped_by_capacity=" << m_ring.DroppedByCapacity() << "\n";

                // 링버퍼가 요청 구간의 앞부분을 이미 버렸다면 그 사실을 반드시 남긴다.
                // 이 값이 참이면 preTriggerUs 만큼의 히스토리를 실제로는 확보하지 못한 것이므로
                // spanUs 또는 maxEvents를 키워야 한다.
                const bool truncated = (!dump.empty() && m_ring.OldestUs() > fromUs);
                meta << "pre_trigger_truncated=" << (truncated ? 1 : 0) << "\n";
            }
        }

        update.dumpWritten = ok;
        update.dumpPath = csvPath;
        update.dumpEventCount = static_cast<long long>(dump.size());
        update.dumpFromUs = fromUs;
        update.dumpToUs = toUs;

        m_dumpPending = false;
        m_havePendingAnchor = false;

        return ok;
    }

    void ShotRecorder::End()
    {
        if (!m_active)
        {
            return;
        }

        if (m_dumpPending)
        {
            // 스트림이 끝났는데 아직 덤프하지 않은 샷이 있으면, 확보한 만큼이라도 저장한다.
            ShotRecorderUpdate dummy;
            FlushPendingDump(dummy, true);
        }

        m_logger.Close();
        m_ring.Clear();
        m_active = false;
    }
}
