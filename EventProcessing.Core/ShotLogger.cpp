#include "pch.h"
#include "ShotLogger.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "Utf8Path.h"

namespace eventcore
{
    ShotLogger::~ShotLogger()
    {
        Close();
    }

    const char* ShotLogger::HeaderLine()
    {
        return
            "window_start_us,window_end_us,window_us,"
            "event_count,on_count,off_count,"
            "detected,center_x,center_y,radius_px,"
            "moment_cx,moment_cy,"
            "area_px,circularity,aspect_ratio,"
            "fit_rms_px,fit_coverage,radius_trusted,fit_points_used,fit_points_total,"
            "contours_total,contours_passed,reject_reason,"
            "shot_state,just_entered_ready,just_triggered,center_speed_px_s";
    }

    bool ShotLogger::Open(const std::string& path)
    {
        Close();

        m_file.open(Utf8ToPath(path), std::ios::binary);

        if (!m_file.is_open())
        {
            std::cerr << "ShotLogger::Open: failed to open " << path << std::endl;
            return false;
        }

        m_file << HeaderLine() << "\n";

        m_open = true;
        m_path = path;
        m_rows = 0;

        return true;
    }

    void ShotLogger::Append(const ShotMeasurement& m)
    {
        if (!m_open)
        {
            return;
        }

        std::ostringstream row;
        row << std::fixed;

        row << m.windowStartUs << ','
            << m.windowEndUs << ','
            << (m.windowEndUs - m.windowStartUs) << ','
            << m.eventCount << ','
            << m.onCount << ','
            << m.offCount << ','
            << (m.detected ? 1 : 0) << ',';

        row << std::setprecision(4)
            << m.centerX << ','
            << m.centerY << ','
            << m.radiusPx << ','
            << m.momentCentroidX << ','
            << m.momentCentroidY << ',';

        row << std::setprecision(3)
            << m.areaPx << ','
            << m.circularity << ','
            << m.aspectRatio << ','
            << m.fitRmsPx << ','
            << m.fitCoverage << ',';

        row << (m.radiusTrusted ? 1 : 0) << ','
            << m.fitPointsUsed << ','
            << m.fitPointsTotal << ','
            << m.contoursTotal << ','
            << m.contoursPassed << ','
            << m.rejectReason << ','
            << m.shotState << ','
            << (m.justEnteredReady ? 1 : 0) << ','
            << (m.justTriggered ? 1 : 0) << ',';

        row << std::setprecision(1) << m.centerSpeedPxPerSec;

        const std::string line = row.str();

        m_file.write(line.data(), static_cast<std::streamsize>(line.size()));
        m_file.put('\n');

        ++m_rows;

        // 샷 도중 앱이 죽어도 이미 기록한 줄은 살아 있어야 하므로 주기적으로 flush한다.
        if ((m_rows % 200) == 0)
        {
            m_file.flush();
        }
    }

    void ShotLogger::Close()
    {
        if (m_file.is_open())
        {
            m_file.flush();
            m_file.close();
        }

        m_open = false;
    }
}
