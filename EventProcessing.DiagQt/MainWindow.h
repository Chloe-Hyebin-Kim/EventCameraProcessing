#pragma once

// 이 프로젝트는 실시간 라이브 카메라 프리뷰가 목적이므로 Metavision SDK가 반드시 필요하다.
// (오프라인 RAW -> 이미지/영상 변환만 필요하면 Metavision SDK 없이도 빌드되는
//  EventProcessing.Console을 대신 사용할 수 있다.)
#include "LiveEventStream.h"
#include "ShotTrigger.h"

#include <QWidget>

#include <deque>
#include <memory>

QT_BEGIN_NAMESPACE
class QAction;
class QGroupBox;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
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
    void onStartPauseClicked();
    void onStopClicked();
    void onBrowseRawClicked();
    void onBrowseOutputClicked();
    void onPollStreamState();
    void onSliderMoved(int value);
    void onSliderReleased();

private:
    // Start/Pause는 버튼 하나를 같이 쓴다(눌린 순간의 m_runState에 따라 동작이 갈림).
    // Idle에서 누르면 StartStream(), Running에서 누르면 PauseStream(), Paused에서 누르면
    // ResumeStream()이 호출된다. Stop은 별도 버튼으로, 항상 처음(IDLE, 까만 화면)으로 되돌린다.
    enum class RunState
    {
        Idle,
        Running,
        Paused,
    };

    // 설정(Settings) 메뉴 > Language에서 고르는 UI 표시 언어. 별도 Qt Linguist(.ts/.qm) 빌드
    // 없이, 문자열마다 영어/한국어 쌍을 코드에 직접 두고 Tr()로 골라 쓰는 가벼운 방식이다.
    enum class AppLanguage
    {
        English,
        Korean,
    };

    void BuildUi();
    eventcore::ShotTriggerConfig ReadConfigFromUI() const;
    void AppendLog(const QString& msg);
    void UpdateStateLabel(eventcore::ShotState state);
    void UpdateRunButtons();
    void DrawFrame(const cv::Mat& bgrFrame);

    // 현재 m_language에 맞는 쪽을 돌려준다. UI 문자열은 모두 이걸 통해서 얻는다.
    QString Tr(const QString& en, const QString& ko) const;
    void SetLanguage(AppLanguage lang);
    // m_language가 바뀔 때마다(그리고 최초 BuildUi() 끝에서 한 번) 모든 위젯의 표시 텍스트를
    // 다시 채운다. 버튼/라벨/툴팁/메뉴 제목 등 위젯 생성 시점의 리터럴 텍스트는 두지 않고,
    // 여기서만 실제 문구를 정한다.
    void RetranslateUi();
    void StartStream();
    void PauseStream();
    void ResumeStream();
    void StartCaptureSave();
    void SaveCaptureFrame(const cv::Mat& bgrFrame);
    void FinishCaptureSave();
    void StopStream(const QString& logMessage);
    void PushPreRollFrame(const std::shared_ptr<FrameMessage>& msg);
    void FlushPreRollBuffer(eventcore::lli impactUs);

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
    eventcore::ShotTriggerConfig m_activeConfig;
    bool m_running = false;
    RunState m_runState = RunState::Idle;
    AppLanguage m_language = AppLanguage::English;
    bool m_liveMode = false;

    // Live 카메라 모드에서만 쓰인다: Pause 동안 카메라/미리보기는 계속 흐르게 두고(끼어든 상황이
    // 지나가는 걸 볼 수 있게), ShotTrigger 갱신과 프레임 저장(녹화)만 건너뛴다. RAW 모드의 Pause는
    // m_stream.Pause()로 재생 자체를 멈추므로 이 플래그와 무관하게 콜백이 아예 오지 않는다.
    bool m_processingPaused = false;

    QTimer* m_pollTimer = nullptr;

    // Impact 확정 이전 프레임들을 preCaptureSeconds만큼 보관해 두는 링 버퍼. Impact가 확정되면
    // (justTriggered) 여기서 기준 프레임 이후분을 한꺼번에 저장하고, 이후 프레임은 Trajectory
    // 상태 동안 실시간으로 저장한다.
    std::deque<std::shared_ptr<FrameMessage>> m_preRollBuffer;

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
    // 설정 메뉴 (BuildUi()가 root 레이아웃에 QLayout::setMenuBar()로 얹는다 - QMainWindow가
    // 아니어도 위젯 하나짜리 앱에 메뉴바를 둘 수 있다).
    QMenu* m_menuSettings = nullptr;
    QMenu* m_menuLanguage = nullptr;
    QAction* m_actionLangEnglish = nullptr;
    QAction* m_actionLangKorean = nullptr;

    QGroupBox* m_boxSource = nullptr;
    QGroupBox* m_boxOutput = nullptr;
    QGroupBox* m_boxShotTrigger = nullptr;

    QRadioButton* m_radioLive = nullptr;
    QRadioButton* m_radioRaw = nullptr;
    QLineEdit* m_editRawPath = nullptr;
    QPushButton* m_btnBrowseRaw = nullptr;
    QLineEdit* m_editOutputDir = nullptr;
    QPushButton* m_btnBrowseOutput = nullptr;

    QLabel* m_labelReadySec = nullptr;
    QLabel* m_labelPreCaptureSec = nullptr;
    QLabel* m_labelPostCaptureSec = nullptr;
    QLabel* m_labelStablePx = nullptr;
    QLabel* m_labelShotSpeed = nullptr;
    QLabel* m_labelDirConsistentFrames = nullptr;
    QLabel* m_labelMaxDirDeviationDeg = nullptr;
    QLabel* m_labelMissToleranceMs = nullptr;
    QLabel* m_labelWindowUs = nullptr;

    QLineEdit* m_editReadySec = nullptr;
    QLineEdit* m_editPreCaptureSec = nullptr;
    QLineEdit* m_editPostCaptureSec = nullptr;
    QLineEdit* m_editStablePx = nullptr;
    QLineEdit* m_editShotSpeed = nullptr;
    QLineEdit* m_editDirConsistentFrames = nullptr;
    QLineEdit* m_editMaxDirDeviationDeg = nullptr;
    QLineEdit* m_editMissToleranceMs = nullptr;
    QLineEdit* m_editWindowUs = nullptr;
    QPushButton* m_btnStartPause = nullptr;
    QPushButton* m_btnStop = nullptr;
    QLabel* m_labelStateCaption = nullptr;
    QLabel* m_labelState = nullptr;
    QLabel* m_labelPreview = nullptr;
    QListWidget* m_listLog = nullptr;
    QSlider* m_sliderPosition = nullptr;
    QLabel* m_labelTime = nullptr;
};
