#include "pch.h"
#include "framework.h"
#include "EventProcessing.Diag.h"
#include "EventProcessingDiagDlg.h"

#include <atlconv.h>
#include <atltime.h>
#include <shlobj.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace eventcore;

namespace
{
    CString FormatShotState(ShotState state)
    {
        switch (state)
        {
        case ShotState::Searching: return _T("SEARCHING");
        case ShotState::Ready:     return _T("READY");
        case ShotState::Capturing: return _T("CAPTURING");
        }
        return _T("?");
    }
}

CEventProcessingDiagDlg::CEventProcessingDiagDlg(CWnd* pParent)
    : CDialogEx(IDD_EVENTPROCESSING_DIAG_DIALOG, pParent)
    , m_hIcon(nullptr)
{
}

void CEventProcessingDiagDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_LOG, m_listLog);
}

BEGIN_MESSAGE_MAP(CEventProcessingDiagDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_DESTROY()
    ON_BN_CLICKED(IDC_BUTTON_START, &CEventProcessingDiagDlg::OnBnClickedButtonStart)
    ON_BN_CLICKED(IDC_BUTTON_STOP, &CEventProcessingDiagDlg::OnBnClickedButtonStop)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE_RAW, &CEventProcessingDiagDlg::OnBnClickedButtonBrowseRaw)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE_OUTPUT, &CEventProcessingDiagDlg::OnBnClickedButtonBrowseOutput)
    ON_MESSAGE(WM_APP_FRAME_READY, &CEventProcessingDiagDlg::OnFrameReady)
END_MESSAGE_MAP()

BOOL CEventProcessingDiagDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 외부 .ico 파일 없이도 빌드되도록 시스템 기본 아이콘 사용
    m_hIcon = ::LoadIcon(nullptr, IDI_APPLICATION);
    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    CheckRadioButton(IDC_RADIO_LIVE, IDC_RADIO_RAW, IDC_RADIO_RAW);

    SetDlgItemText(IDC_EDIT_OUTPUTDIR, _T(".\\output"));
    SetDlgItemText(IDC_EDIT_READY_SEC, _T("0.4"));
    SetDlgItemText(IDC_EDIT_CAPTURE_SEC, _T("2.5"));
    SetDlgItemText(IDC_EDIT_STABLE_PX, _T("15"));
    SetDlgItemText(IDC_EDIT_SHOT_SPEED, _T("1000"));
    SetDlgItemText(IDC_EDIT_MISS_TOLERANCE_MS, _T("150"));
    SetDlgItemText(IDC_EDIT_WINDOW_US, _T("10000"));

    GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);
    SetDlgItemText(IDC_STATIC_STATE, _T("IDLE"));

    InitTriggerSettingsTooltips();

    return TRUE;
}

BOOL CEventProcessingDiagDlg::PreTranslateMessage(MSG* pMsg)
{
    if (m_toolTip.GetSafeHwnd() != nullptr)
    {
        m_toolTip.RelayEvent(pMsg);
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}

void CEventProcessingDiagDlg::InitTriggerSettingsTooltips()
{
    m_toolTip.Create(this, TTS_ALWAYSTIP);
    m_toolTip.SetMaxTipWidth(280);
    m_toolTip.Activate(TRUE);

    struct TooltipEntry
    {
        UINT labelId;
        UINT editId;
        LPCTSTR text;
    };

    static const TooltipEntry entries[] = {
        { IDC_STATIC_READY_SEC, IDC_EDIT_READY_SEC,
          _T("공 중심점이 Stable jitter 이내로 이만큼(초) 정지해 있으면 READY 상태로 인정합니다.") },
        { IDC_STATIC_CAPTURE_SEC, IDC_EDIT_CAPTURE_SEC,
          _T("샷이 트리거된 후 이 시간(초) 동안 촬영을 유지한 뒤 자동으로 Searching으로 복귀합니다.") },
        { IDC_STATIC_STABLE_PX, IDC_EDIT_STABLE_PX,
          _T("READY 판정 중 허용하는 공 중심점의 흔들림 범위(px). 이보다 더 움직이면 정지 타이머가 리셋됩니다.") },
        { IDC_STATIC_SHOT_SPEED, IDC_EDIT_SHOT_SPEED,
          _T("READY 상태에서 공 중심점이 이 속도(px/s) 이상으로 움직이면 샷으로 판단해 촬영을 트리거합니다.") },
        { IDC_STATIC_MISS_TOLERANCE_MS, IDC_EDIT_MISS_TOLERANCE_MS,
          _T("공 검출이 이 시간(ms) 이내로 잠깐 끊겨도 상태를 리셋하지 않고 유지합니다.") },
        { IDC_STATIC_WINDOW_US, IDC_EDIT_WINDOW_US,
          _T("이벤트를 한 프레임으로 누적하는 시간 간격(us). 이 주기마다 공 검출/분석을 수행합니다.") },
    };

    for (const TooltipEntry& entry : entries)
    {
        if (CWnd* label = GetDlgItem(entry.labelId))
        {
            m_toolTip.AddTool(label, entry.text);
        }
        if (CWnd* edit = GetDlgItem(entry.editId))
        {
            m_toolTip.AddTool(edit, entry.text);
        }
    }
}

void CEventProcessingDiagDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this);
        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
    }
}

HCURSOR CEventProcessingDiagDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CEventProcessingDiagDlg::OnDestroy()
{
    m_stream.Stop();
    CDialogEx::OnDestroy();
}

ShotTriggerConfig CEventProcessingDiagDlg::ReadConfigFromUI() const
{
    ShotTriggerConfig cfg;
    CString s;

    GetDlgItemText(IDC_EDIT_READY_SEC, s);
    cfg.readySeconds = _ttof(s);

    GetDlgItemText(IDC_EDIT_CAPTURE_SEC, s);
    cfg.captureSeconds = _ttof(s);

    GetDlgItemText(IDC_EDIT_STABLE_PX, s);
    cfg.stableMovePx = static_cast<float>(_ttof(s));

    GetDlgItemText(IDC_EDIT_SHOT_SPEED, s);
    cfg.shotSpeedPxPerSec = static_cast<float>(_ttof(s));

    GetDlgItemText(IDC_EDIT_MISS_TOLERANCE_MS, s);
    cfg.missToleranceUs = static_cast<lli>(_ttof(s) * 1000.0);

    return cfg;
}

void CEventProcessingDiagDlg::AppendLog(const CString& msg)
{
    CTime now = CTime::GetCurrentTime();
    CString line;
    line.Format(_T("[%02d:%02d:%02d] %s"), now.GetHour(), now.GetMinute(), now.GetSecond(), (LPCTSTR)msg);

    m_listLog.AddString(line);
    m_listLog.SetTopIndex(m_listLog.GetCount() - 1);
}

void CEventProcessingDiagDlg::UpdateStateLabel(ShotState state)
{
    SetDlgItemText(IDC_STATIC_STATE, FormatShotState(state));
}

void CEventProcessingDiagDlg::DrawFrame(const cv::Mat& bgrFrame)
{
    if (bgrFrame.empty() || bgrFrame.type() != CV_8UC3)
    {
        return;
    }

    CWnd* preview = GetDlgItem(IDC_STATIC_PREVIEW);
    if (preview == nullptr)
    {
        return;
    }

    CRect rc;
    preview->GetClientRect(&rc);
    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    CDC* pDC = preview->GetDC();
    if (pDC == nullptr)
    {
        return;
    }

    const cv::Mat safe = bgrFrame.isContinuous() ? bgrFrame : bgrFrame.clone();

    const double scale = std::min(
        static_cast<double>(rc.Width()) / safe.cols,
        static_cast<double>(rc.Height()) / safe.rows);
    const int dw = std::max(1, static_cast<int>(safe.cols * scale));
    const int dh = std::max(1, static_cast<int>(safe.rows * scale));
    const int dx = (rc.Width() - dw) / 2;
    const int dy = (rc.Height() - dh) / 2;

    pDC->FillSolidRect(&rc, RGB(0, 0, 0));

    BITMAPINFOHEADER bih = {};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = safe.cols;
    bih.biHeight = -safe.rows; // top-down DIB (cv::Mat row 0 is the top row)
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    ::StretchDIBits(
        pDC->GetSafeHdc(),
        dx, dy, dw, dh,
        0, 0, safe.cols, safe.rows,
        safe.data,
        reinterpret_cast<BITMAPINFO*>(&bih),
        DIB_RGB_COLORS,
        SRCCOPY);

    preview->ReleaseDC(pDC);
}

void CEventProcessingDiagDlg::StartCaptureSave()
{
    CTime now = CTime::GetCurrentTime();
    CString folder;
    folder.Format(_T("%s\\shot_%04d%02d%02d_%02d%02d%02d"),
        (LPCTSTR)m_outputDir,
        now.GetYear(), now.GetMonth(), now.GetDay(),
        now.GetHour(), now.GetMinute(), now.GetSecond());

    ::CreateDirectory(m_outputDir, nullptr);
    ::CreateDirectory(folder, nullptr);

    m_currentCaptureDir = folder;
    m_captureFrameIndex = 0;
    m_capturingNow = true;
}

void CEventProcessingDiagDlg::SaveCaptureFrame(const cv::Mat& bgrFrame)
{
    if (!m_capturingNow || bgrFrame.empty())
    {
        return;
    }

    CString filename;
    filename.Format(_T("%s\\frame_%04d.png"), (LPCTSTR)m_currentCaptureDir, m_captureFrameIndex);

    // cv::imwrite expects an ANSI(system codepage)-encoded path on Windows, so convert
    // with the default codepage rather than forcing UTF-8 (output folder may contain
    // non-ASCII characters, e.g. a Korean Windows user profile path).
    CT2A ansiPath(filename);
    cv::imwrite(std::string(ansiPath), bgrFrame);

    ++m_captureFrameIndex;
}

void CEventProcessingDiagDlg::FinishCaptureSave()
{
    m_capturingNow = false;
}

void CEventProcessingDiagDlg::OnBnClickedButtonStart()
{
    if (m_running)
    {
        return;
    }

    CString rawPath, outputDir, windowStr;
    GetDlgItemText(IDC_EDIT_RAWPATH, rawPath);
    GetDlgItemText(IDC_EDIT_OUTPUTDIR, outputDir);
    GetDlgItemText(IDC_EDIT_WINDOW_US, windowStr);

    m_outputDir = outputDir;
    ::CreateDirectory(m_outputDir, nullptr);

    lli windowUs = _ttoll(windowStr);
    if (windowUs <= 0)
    {
        windowUs = 10000;
    }

    m_trigger = ShotTrigger(ReadConfigFromUI());
    m_capturingNow = false;
    m_captureFrameIndex = 0;

    const bool live = (IsDlgButtonChecked(IDC_RADIO_LIVE) == BST_CHECKED);
    CT2A rawPathAnsi(rawPath);
    const std::string rawPathStd(static_cast<LPCSTR>(rawPathAnsi));

    if (!live && rawPathStd.empty())
    {
        AppendLog(_T("Please choose a RAW file, or select 'Live camera'."));
        return;
    }

    const HWND hWnd = GetSafeHwnd();

    // 콜백은 워커 스레드에서 호출된다. this를 캡처하지 않고(다이얼로그 수명과 무관하게 안전한)
    // 순수 HWND + 힙 할당 메시지 페이로드만 사용해 PostMessage로 UI 스레드에 마샬링한다.
    const bool ok = m_stream.Start(
        live ? "" : rawPathStd.c_str(),
        windowUs,
        [hWnd](const EventProcessingResult& result, lli startUs, lli endUs)
        {
            FrameMessage* msg = new FrameMessage();
            msg->frame = result.debugImage.clone();
            msg->ball = result.ball;
            msg->windowStartUs = startUs;
            msg->windowEndUs = endUs;
            ::PostMessage(hWnd, WM_APP_FRAME_READY, 0, reinterpret_cast<LPARAM>(msg));
        });

    if (!ok)
    {
        CString msg = _T("Failed to start stream");
        const std::string& err = m_stream.LastError();
        if (!err.empty())
        {
            CA2T errT(err.c_str());
            msg.AppendFormat(_T(": %s"), static_cast<LPCTSTR>(errT));
        }
        AppendLog(msg);
        return;
    }

    m_running = true;
    GetDlgItem(IDC_BUTTON_START)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(TRUE);
    SetDlgItemText(IDC_STATIC_STATE, _T("SEARCHING"));
    AppendLog(live ? _T("Started (live camera)") : _T("Started (RAW playback)"));
}

void CEventProcessingDiagDlg::OnBnClickedButtonStop()
{
    if (!m_running)
    {
        return;
    }

    m_stream.Stop();
    m_running = false;

    GetDlgItem(IDC_BUTTON_START)->EnableWindow(TRUE);
    GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);
    SetDlgItemText(IDC_STATIC_STATE, _T("IDLE"));
    AppendLog(_T("Stopped"));
}

void CEventProcessingDiagDlg::OnBnClickedButtonBrowseRaw()
{
    CFileDialog dlg(TRUE, _T("raw"), nullptr,
        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        _T("Metavision RAW (*.raw)|*.raw|All Files (*.*)|*.*||"));

    if (dlg.DoModal() == IDOK)
    {
        SetDlgItemText(IDC_EDIT_RAWPATH, dlg.GetPathName());
        CheckRadioButton(IDC_RADIO_LIVE, IDC_RADIO_RAW, IDC_RADIO_RAW);
    }
}

void CEventProcessingDiagDlg::OnBnClickedButtonBrowseOutput()
{
    TCHAR path[MAX_PATH] = { 0 };

    BROWSEINFO bi = {};
    bi.hwndOwner = GetSafeHwnd();
    bi.lpszTitle = _T("Select output folder");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
    if (pidl != nullptr)
    {
        if (::SHGetPathFromIDList(pidl, path))
        {
            SetDlgItemText(IDC_EDIT_OUTPUTDIR, path);
        }
        ::CoTaskMemFree(pidl);
    }
}

LRESULT CEventProcessingDiagDlg::OnFrameReady(WPARAM, LPARAM lParam)
{
    std::unique_ptr<FrameMessage> msg(reinterpret_cast<FrameMessage*>(lParam));
    if (!msg || msg->frame.empty())
    {
        return 0;
    }

    DrawFrame(msg->frame);

    const ShotUpdateResult su = m_trigger.Update(msg->ball, msg->windowStartUs);
    UpdateStateLabel(su.state);

    if (su.justEnteredReady)
    {
        AppendLog(_T("READY"));
    }

    if (su.justTriggered)
    {
        StartCaptureSave();
        AppendLog(_T("TRIGGERED - capture started"));
    }

    if (su.state == ShotState::Capturing)
    {
        SaveCaptureFrame(msg->frame);
    }

    if (su.justFinishedCapture)
    {
        CString line;
        line.Format(_T("Capture finished: %d frame(s) saved to %s"), m_captureFrameIndex, (LPCTSTR)m_currentCaptureDir);
        AppendLog(line);
        FinishCaptureSave();
    }

    return 0;
}
