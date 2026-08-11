#pragma once

// 이 프로젝트는 실시간 라이브 카메라 프리뷰가 목적이므로 Metavision SDK가 반드시 필요하다.
// (오프라인 RAW -> 이미지/영상 변환만 필요하면 Metavision SDK 없이도 빌드되는
//  EventProcessing.Console을 대신 사용할 수 있다.)
#include "LiveEventStream.h"
#include "ShotTrigger.h"

#define WM_APP_FRAME_READY (WM_APP + 100)

struct FrameMessage
{
    cv::Mat frame;
    eventcore::BallDetectionResult ball;
    eventcore::lli windowStartUs = 0;
    eventcore::lli windowEndUs = 0;
};

class CEventProcessingDiagDlg : public CDialogEx
{
public:
    explicit CEventProcessingDiagDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_EVENTPROCESSING_DIAG_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnDestroy();
    afx_msg void OnBnClickedButtonStart();
    afx_msg void OnBnClickedButtonStop();
    afx_msg void OnBnClickedButtonBrowseRaw();
    afx_msg void OnBnClickedButtonBrowseOutput();
    afx_msg LRESULT OnFrameReady(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    eventcore::ShotTriggerConfig ReadConfigFromUI() const;
    void AppendLog(const CString& msg);
    void UpdateStateLabel(eventcore::ShotState state);
    void DrawFrame(const cv::Mat& bgrFrame);
    void StartCaptureSave();
    void SaveCaptureFrame(const cv::Mat& bgrFrame);
    void FinishCaptureSave();

    HICON m_hIcon;

    eventcore::LiveEventStream m_stream;
    eventcore::ShotTrigger m_trigger;
    bool m_running = false;

    CString m_outputDir;
    CString m_currentCaptureDir;
    int m_captureFrameIndex = 0;
    bool m_capturingNow = false;

    CListBox m_listLog;
};
