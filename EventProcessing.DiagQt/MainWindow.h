#pragma once

// 이 프로젝트는 실시간 라이브 카메라 프리뷰가 목적이므로 Metavision SDK가 반드시 필요하다.
// (오프라인 RAW -> 이미지/영상 변환만 필요하면 Metavision SDK 없이도 빌드되는
//  EventProcessing.Console을 대신 사용할 수 있다.)
#include "LiveEventStream.h"
#include "ShotTrigger.h"

#include <QWidget>

#include <memory>

QT_BEGIN_NAMESPACE
class QKeyEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSlider;
class QTimer;
QT_END_NAMESPACE

struct FrameMessage
{
    cv::Mat frame;
    eventcore::BallDetectionResult ball;
    eventcore::lli windowStartUs = 0;
    eventcore::lli windowEndUs = 0;
};

// Qt Widgets 기반 Live/RAW Diagnostic Viewer. Windows/Linux(및 다른 Qt 지원 플랫폼)에서
// 동일한 소스로 빌드/실행할 수 있다.
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onBrowseRawClicked();
    void onBrowseOutputClicked();
    void onPollStreamState();
    void onSliderMoved(int value);
    void onSliderReleased();

private:
    void BuildUi();
    eventcore::ShotTriggerConfig ReadConfigFromUI() const;
    void AppendLog(const QString& msg);
    void UpdateStateLabel(eventcore::ShotState state);
    void DrawFrame(const cv::Mat& bgrFrame);
    void StartCaptureSave();
    void SaveCaptureFrame(const cv::Mat& bgrFrame);
    void FinishCaptureSave();
    void StopStream(const QString& logMessage);

    // LiveEventStream의 콜백은 워커 스레드에서 호출된다. 캡처한 프레임은 힙에 올려
    // QMetaObject::invokeMethod(..., Qt::QueuedConnection)로 UI 스레드에 마샬링해서 처리한다.
    void OnFrameReady(std::shared_ptr<FrameMessage> msg);

    void SeekTo(eventcore::lli timestampUs);
    eventcore::lli SliderValueToTimestamp(int value) const;
    int TimestampToSliderValue(eventcore::lli timestampUs) const;
    void UpdateTimeLabel(eventcore::lli currentUs);
    static QString FormatTimeUs(eventcore::lli us);

    eventcore::LiveEventStream m_stream;
    eventcore::ShotTrigger m_trigger;
    bool m_running = false;
    QTimer* m_pollTimer = nullptr;

    // RAW 파일 탐색(seek) 관련 상태. Live 카메라 소스에서는 사용하지 않는다.
    // Start() 직후에는 카메라가 아직 완전히 준비되지 않아 offline_streaming_control() 호출이
    // (문서화된 CameraException을 넘어) 실제 메모리 접근 위반까지 일으키는 경우가 있었으므로,
    // 실제 프레임을 한 번이라도 받아 스트림이 정말 살아있는 게 확인되기 전에는 건드리지 않는다.
    bool m_gotFirstFrame = false;
    bool m_seekRangeKnown = false;
    eventcore::lli m_seekStartUs = 0;
    eventcore::lli m_seekEndUs = 0;

    QString m_outputDir;
    QString m_currentCaptureDir;
    int m_captureFrameIndex = 0;
    bool m_capturingNow = false;

    QPixmap m_previewPixmap;

    // UI
    QRadioButton* m_radioLive = nullptr;
    QRadioButton* m_radioRaw = nullptr;
    QLineEdit* m_editRawPath = nullptr;
    QPushButton* m_btnBrowseRaw = nullptr;
    QLineEdit* m_editOutputDir = nullptr;
    QPushButton* m_btnBrowseOutput = nullptr;
    QLineEdit* m_editReadySec = nullptr;
    QLineEdit* m_editCaptureSec = nullptr;
    QLineEdit* m_editStablePx = nullptr;
    QLineEdit* m_editShotSpeed = nullptr;
    QLineEdit* m_editMissToleranceMs = nullptr;
    QLineEdit* m_editWindowUs = nullptr;
    QPushButton* m_btnStart = nullptr;
    QPushButton* m_btnStop = nullptr;
    QLabel* m_labelState = nullptr;
    QLabel* m_labelPreview = nullptr;
    QListWidget* m_listLog = nullptr;
    QSlider* m_sliderPosition = nullptr;
    QLabel* m_labelTime = nullptr;
};
