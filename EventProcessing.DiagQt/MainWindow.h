#pragma once

// 이 프로젝트는 실시간 라이브 카메라 프리뷰가 목적이므로 Metavision SDK가 반드시 필요하다.
// (오프라인 RAW -> 이미지/영상 변환만 필요하면 Metavision SDK 없이도 빌드되는
//  EventProcessing.Console을 대신 사용할 수 있다.)
#include "LiveEventStream.h"
#include "ShotTrigger.h"
#include "CalibrationTypes.h"       // CheckerboardConfig (값 반환 헬퍼가 완전한 타입을 필요로 함)
#include "CheckerboardDetector.h"   // CheckerboardDetection (멤버로 값 보관)
#include "CalibrationObservation.h" // CalibrationSampleCollector (멤버로 값 보관)

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace eventcore
{
    class CalibrationImageBuilder;  // Calibration/이미지 빌더(원본 event 누적). 구현은 .cpp에서 include.
}

QT_BEGIN_NAMESPACE
class QAction;
class QCheckBox;
class QFormLayout;
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
class QWidget;
QT_END_NAMESPACE

struct FrameMessage
{
    cv::Mat frame;
    eventcore::BallDetectionResult ball;
    eventcore::lli windowStartUs = 0;
    eventcore::lli windowEndUs = 0;

    // Calibration Mode일 때만 채워진다(그 외에는 빈 벡터로 두어 불필요한 복사를 피한다).
    // 원본 event를 UI 스레드의 CalibrationImageBuilder로 넘기기 위한 스냅샷.
    std::vector<eventcore::Event> events;
};

// 워커 스레드에서 프레임 처리를 마친 뒤 UI 스레드로 넘기는 "표시용" 데이터.
// 프레임(preview)은 최신 것만 의미가 있어 coalesce(덮어쓰기)되지만, 로그 줄은 유실되면 안 되므로
// 누적된다. UI 스레드는 이 데이터로 화면/라벨/로그만 갱신하고, 처리 로직은 전혀 수행하지 않는다.
struct WorkerUiUpdate
{
    bool hasFrame = false;
    cv::Mat frame;                 // BGR(오버레이 적용됨). preview 표시용.
    eventcore::lli windowStartUs = 0;

    bool hasShotState = false;
    eventcore::ShotState shotState = eventcore::ShotState::Searching;

    bool hasCalibStatus = false;
    QString calibStatus;

    QStringList logs;              // 이번 배치에서 남길 로그 줄(누적, 유실 금지).
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
    void onStartStopClicked();
    void onPauseResumeClicked();
    void onRecordSaveClicked();
    void onBrowseRawClicked();
    void onBrowseOutputClicked();
    void onPollStreamState();
    void onSliderMoved(int value);
    void onSliderReleased();
    void onShowAboutVersion();
    void onShowAboutLicense();
    // Live/RAW 모드 라디오가 바뀔 때. 소스 텍스트 박스를 모드에 맞게 갈아 끼운다.
    void onSourceModeToggled(bool liveChecked);
    // 현재 카메라 bias 조합을 .bias 파일로 저장 / 파일에서 불러와 적용.
    void onSaveBiasClicked();
    void onLoadBiasClicked();
    // Calibration Mode 체크박스 토글. 켜지면 원본 event를 누적해 calibration 이미지를 화면에
    // 표시하고(ShotTrigger/BallDetector 경로는 건너뜀), 끄면 기존 동작으로 복귀한다.
    void onCalibrationModeToggled(bool checked);
    // Calibration observation 수집(수동 Capture 방식): 현재(가장 최근 완성) 프레임의 검출 결과를
    // 하나의 observation으로 저장 / 마지막 것 제거 / 전체 초기화.
    void onCaptureSampleClicked();
    void onRemoveLastSampleClicked();
    void onClearSamplesClicked();

private:
    // 버튼 두 개, 각각 두 가지 역할을 겸한다:
    // - Start(시작)/Stop(중단) 버튼: Idle에서 누르면 StartStream(), Running/Paused에서 누르면
    //   StopStream()(항상 처음 IDLE, 까만 화면으로 되돌아감).
    // - Pause(멈춤)/Resume(재개) 버튼: Running에서 누르면 PauseStream(), Paused에서 누르면
    //   ResumeStream(). Idle일 때는 비활성화.
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

    // Camera Bias 슬라이더 한 행의 위젯들(아래 private 멤버 목록에 전체 정의가 있음) -
    // CreateBiasRow()/ApplyBiasTooltip() 등 이 밑의 메서드 선언들이 먼저 참조하므로 전방 선언.
    struct BiasControlRow;

    // 소스 텍스트 박스(m_editRawPath)를 현재 모드/연결 상태에 맞게 갱신한다:
    // - RAW 모드: 편집 가능, 마지막으로 고른 RAW 파일 경로를 표시.
    // - Live 모드: 읽기 전용. 연결(Start)되면 카메라 식별자, 아니면 안내 문구.
    void UpdateSourceBox();
    // 현재 연결된 카메라의 식별 문자열(시리얼 번호 + 세대/통합사)을 만든다. 연결 안 됐으면 빈 문자열.
    QString FormatCameraIdentifier() const;

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
    // 수동 녹화(자동 샷 캡처와 별개). Record 버튼으로 시작, Save 버튼/Stop으로 종료한다.
    void StartManualRecord();
    void SaveManualFrame(const cv::Mat& bgrFrame);
    void StopManualRecord();
    void StopStream(const QString& logMessage);
    void PushPreRollFrame(const std::shared_ptr<FrameMessage>& msg);
    void FlushPreRollBuffer(eventcore::lli impactUs, WorkerUiUpdate& upd);

    // 앱 시작 시 한 번, IMX636의 알려진 표준 bias 6종에 대해 비활성화된 슬라이더 행을 미리
    // 만들어 둔다(연결 전이라 실제 값/범위를 모르므로 자리표시자 상태). BuildUi()에서 호출.
    void SeedBiasPlaceholders();
    // biasName 하나에 대한 슬라이더 행(컨테이너+슬라이더+값 라벨)을 지정한 열(form)에 추가하고
    // 돌려준다(m_biasRows에 넣는 건 호출부 책임). dynamic=true는 알려진 6종에 없는, 실제 연결된
    // 카메라가 보고한 추가 bias용(연결 해제 시 제거 대상)이라는 표시.
    BiasControlRow CreateBiasRow(const QString& biasName, bool dynamic, QFormLayout* form);
    // bias 이름을 어느 열(form)에 둘지 결정한다(왼쪽: diff/diff_off/diff_on, 그 외: 오른쪽).
    QFormLayout* FormColumnForBias(const QString& biasName) const;
    // row의 현재 언어 설명에 맞는 툴팁을 슬라이더/컨테이너/이름 라벨에 다시 적용한다.
    void ApplyBiasTooltip(const BiasControlRow& row);
    // 연결 상태(m_biasConnected)와 현재 언어에 맞춰 안내 라벨 문구를 갱신한다.
    void UpdateBiasStatusLabel();

    // 라이브 카메라가 성공적으로 시작된 뒤 m_stream.GetBiases()로 얻은 실제 값/범위로 기존
    // placeholder 행들을 갱신하고 활성화한다(알려진 6종에 없는 이름은 새 행을 동적으로 추가).
    // applySaved=true면 기억해 둔 값(기본값/사용자 변경분)을 카메라에 적용, false면 카메라가
    // 현재 들고 있는 값을 그대로 반영만 하고 기억 값도 그 값으로 맞춘다(.bias 로드 직후 등).
    void PopulateBiasControls(bool applySaved = true);
    // 알려진 6종 행은 비활성화 + 자리표시자 상태로 되돌리고(삭제하지 않음), 동적으로 추가됐던
    // 행만 제거한다(Stop, 또는 RAW 모드로 시작할 때).
    void ClearBiasControls();

    // LiveEventStream의 콜백(워커 스레드)에서 직접 호출된다. 모든 프레임 처리(샷 트리거/pre-roll/
    // 녹화/calibration 누적·검출)를 워커 스레드에서 수행하여 UI 스레드를 절대 막지 않는다.
    // 처리 결과 중 화면 표시에 필요한 데이터만 WorkerUiUpdate로 만들어 UI 스레드에 coalesce 전달한다.
    // m_procMutex로 처리 상태를 보호한다.
    void HandleFrameOnWorker(const eventcore::EventProcessingResult& result,
                             const std::vector<eventcore::Event>& events,
                             eventcore::lli startUs, eventcore::lli endUs);
    // WorkerUiUpdate를 coalesced 우편함(m_pendingUi)에 병합하고, 아직 예약된 게 없으면
    // ApplyPendingUi() 호출을 UI 스레드에 1건 예약한다(m_uiMutex 보호). 워커 스레드에서 호출.
    void PostUiUpdate(WorkerUiUpdate&& update);
    // UI 스레드에서 실행: 우편함의 최신 업데이트 1건을 꺼내 화면/라벨/로그에 반영한다.
    void ApplyPendingUi();

    // m_procMutex를 이미 쥔 상태에서 호출한다: config snapshot으로 CalibrationImageBuilder를 새로 만든다.
    // (UI 위젯을 만지지 않으므로 워커 스레드에서도 안전하다.)
    void RebuildCalibBuilderLocked();
    // UI 스레드에서 호출: 현재 UI의 Accumulation(ms)/Checkerboard 설정을 snapshot에 반영한다
    // (m_procMutex로 보호). 워커 스레드는 이 snapshot만 읽는다.
    void SnapshotCalibConfigFromUi();
    // Checkerboard(rows/cols/square mm) 설정을 UI에서 읽는다.
    eventcore::CheckerboardConfig ReadCheckerboardConfigFromUI() const;
    // 수집된 샘플 개수/버튼 활성화 상태 등 calibration observation UI를 갱신한다.
    void UpdateCalibrationSampleUi();

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

    // Calibration Mode 상태. m_calibrationMode는 워커 스레드(Start 콜백)에서 읽고 UI 스레드에서
    // 쓰므로 atomic으로 둔다. 원본 event 복사는 이 플래그가 켜져 있을 때만 수행한다.
    std::atomic<bool> m_calibrationMode{ false };
    std::unique_ptr<eventcore::CalibrationImageBuilder> m_calibBuilder;
    int m_calibImageCount = 0;  // 이번 실행에서 완성된 calibration 이미지 수(상태 표시용)

    // 수동 Capture를 위해 "가장 최근에 완성된 calibration 이미지"의 검출 결과를 캐시한다.
    // Capture 버튼은 이 캐시를 하나의 observation으로 저장한다.
    eventcore::CheckerboardDetection m_lastCalibDetection;
    eventcore::CheckerboardConfig m_lastCalibConfig;
    eventcore::lli m_lastCalibFrameUs = 0;
    bool m_haveLastCalibDetection = false;
    // 가장 최근 완성된 calibration 이미지(CV_8UC1)의 사본. Capture 시 정밀(thorough) 재검출에 쓴다.
    cv::Mat m_lastCalibImage;

    // 수집된 calibration observation(여러 pose). Start/Stop을 반복해도 세션 동안 유지된다(Clear로만 비움).
    eventcore::CalibrationSampleCollector m_calibSamples;

    // 마지막으로 로그에 남긴 샷 상태. 매 프레임 ShotTrigger가 돌려주는 상태가 이 값과 다르면
    // 상태 전이로 보고 로그에 한 줄 남긴다(READY뿐 아니라 SEARCHING/IMPACT/TRJCT 전이 모두).
    eventcore::ShotState m_lastLoggedState = eventcore::ShotState::Searching;

    // Live 카메라 모드에서만 쓰인다: Pause 동안 카메라/미리보기는 계속 흐르게 두고(끼어든 상황이
    // 지나가는 걸 볼 수 있게), ShotTrigger 갱신과 프레임 저장(녹화)만 건너뛴다. RAW 모드의 Pause는
    // m_stream.Pause()로 재생 자체를 멈추므로 이 플래그와 무관하게 콜백이 아예 오지 않는다.
    std::atomic<bool> m_processingPaused{ false };

    QTimer* m_pollTimer = nullptr;

    // 워커 프레임 처리 파이프라인(스레드 경계) --------------------------------------------------
    // 워커 스레드가 만지는 처리 상태(m_trigger, m_preRollBuffer, 캡처/녹화 상태, calibration
    // 빌더/캐시 등)를 보호한다. UI 스레드에서 이 상태를 건드리는 조작(Capture/Clear/모드 토글/
    // 녹화 시작·정지 등)도 이 뮤텍스를 짧게 쥔다. 이 뮤텍스를 쥔 채로 m_stream.Start/Stop/Pause를
    // 호출하지 않는다(워커가 이 뮤텍스를 기다리는 중일 수 있어 데드락).
    std::mutex m_procMutex;

    // UI로 넘길 표시 데이터의 coalesced 우편함. m_uiMutex로 보호. m_uiPosted가 true면 이미
    // ApplyPendingUi() 호출이 UI 큐에 예약되어 있다는 뜻(추가 예약을 막아 큐가 쌓이지 않게 한다).
    std::mutex m_uiMutex;
    WorkerUiUpdate m_pendingUi;
    bool m_uiPosted = false;

    // UI에서 snapshot한 calibration 설정(워커 스레드는 UI 위젯 대신 이 값을 읽는다). m_procMutex 보호.
    eventcore::CheckerboardConfig m_cbConfigSnapshot;
    eventcore::lli m_accumUsSnapshot = 50000;

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

    // 수동 녹화(Record/Save 버튼) 상태. 자동 샷 캡처(위 m_capturingNow)와 독립적으로 동작한다.
    bool m_manualRecording = false;
    QString m_manualRecordDir;
    int m_manualRecordFrameIndex = 0;

    QPixmap m_previewPixmap;
    // 프리뷰 페인트 throttle용: 마지막으로 실제 화면을 그린 시각(ms, epoch). 0이면 아직 없음.
    qint64 m_lastDrawMs = 0;

    // UI
    // 메뉴바(BuildUi()가 root 레이아웃에 QLayout::setMenuBar()로 얹는다 - QMainWindow가 아니어도
    // 위젯 하나짜리 앱에 메뉴바를 둘 수 있다). 순서: 파일(File) - 설정(Settings) - 정보(About).
    QMenu* m_menuFile = nullptr;
    QMenu* m_menuMode = nullptr;
    QAction* m_actionModeLive = nullptr;
    QAction* m_actionModeRaw = nullptr;
    QAction* m_actionOpenFile = nullptr;
    QAction* m_actionSetOutputPath = nullptr;

    QMenu* m_menuSettings = nullptr;
    QMenu* m_menuLanguage = nullptr;
    QAction* m_actionLangEnglish = nullptr;
    QAction* m_actionLangKorean = nullptr;

    QMenu* m_menuAbout = nullptr;
    QAction* m_actionAboutVersion = nullptr;
    QAction* m_actionAboutLicense = nullptr;

    QGroupBox* m_boxSource = nullptr;
    QGroupBox* m_boxOutput = nullptr;
    QGroupBox* m_boxShotTrigger = nullptr;

    // Calibration Mode UI(Phase 1: 모드 토글 + 누적 시간 + 상태 표시). Capture/Run 등 나머지 버튼은
    // 이후 Phase에서 추가한다.
    QGroupBox* m_boxCalibration = nullptr;
    QCheckBox* m_checkCalibMode = nullptr;
    QLabel* m_labelAccumMs = nullptr;
    QLineEdit* m_editAccumMs = nullptr;
    QLabel* m_labelCalibStatus = nullptr;

    // Checkerboard 설정(Phase 2): 내부 코너 행/열 수, 한 칸 크기(mm).
    QLabel* m_labelCheckerboard = nullptr;
    QLabel* m_labelCbRows = nullptr;
    QLineEdit* m_editCbRows = nullptr;
    QLabel* m_labelCbCols = nullptr;
    QLineEdit* m_editCbCols = nullptr;
    QLabel* m_labelCbSquareMm = nullptr;
    QLineEdit* m_editCbSquareMm = nullptr;

    // Observation 수집 컨트롤(Phase 3).
    QPushButton* m_btnCaptureSample = nullptr;
    QPushButton* m_btnRemoveLastSample = nullptr;
    QPushButton* m_btnClearSamples = nullptr;
    QLabel* m_labelSamples = nullptr;

    // Camera Bias (Metavision HAL I_LL_Biases). IMX636의 알려진 표준 bias 6종은 앱 시작 시부터
    // 비활성화된 자리표시자 슬라이더로 항상 보이고(SeedBiasPlaceholders()), 라이브 카메라가
    // 성공적으로 시작되면 PopulateBiasControls()가 실제 값/범위로 갱신하며 활성화한다.
    QGroupBox* m_boxBias = nullptr;
    // bias 슬라이더를 2열로 배치한다: 왼쪽 열(bias_diff/off/on), 오른쪽 열(bias_fo/hpf/refr).
    // 알려진 6종 밖의 이름(다른 센서/펌웨어)은 오른쪽 열에 이어 붙인다.
    QFormLayout* m_biasFormLeft = nullptr;
    QFormLayout* m_biasFormRight = nullptr;
    QLabel* m_labelBiasUnavailable = nullptr;
    // 현재 bias 조합을 .bias 파일로 저장/불러오기(연결 중일 때만 활성화).
    QPushButton* m_btnSaveBias = nullptr;
    QPushButton* m_btnLoadBias = nullptr;
    // 라이브 카메라가 현재 연결되어 bias 값이 실제 하드웨어를 반영 중인지(false면 자리표시자).
    bool m_biasConnected = false;

    // 프로그램이 실행되는 동안(종료 전까지) 기억할 bias 값. Start->Stop->Start를 반복해도
    // 여기 있는 값이 유지되어, 카메라를 다시 열 때 그대로 다시 적용된다(디스크에 저장하지 않으므로
    // 프로그램을 종료하면 사라짐). 앱 시작 시 bias_diff/off/on/fo의 기본값이 미리 들어 있고,
    // 사용자가 슬라이더를 움직이면 그 값으로 갱신된다.
    QMap<QString, int> m_savedBiasValues;

    struct BiasControlRow
    {
        QString biasName;
        QString englishDescription; // HAL이 보고한 원문(연결 전에는 알려진 bias에 대한 일반 설명으로 대체).
        QString koreanDescription;  // 알려진 표준 bias 이름에 대해서만 채워짐(비어 있으면 영어로 대체).
        QWidget* container = nullptr; // slider + value label을 담는 한 행. 부모는 m_boxBias.
        QSlider* slider = nullptr;
        QLabel* valueLabel = nullptr;
        QWidget* nameLabel = nullptr; // QFormLayout::labelForField()로 얻은, 행의 이름 라벨.
        QFormLayout* form = nullptr;  // 이 행이 속한 열(왼쪽/오른쪽 form). removeRow/labelForField에 사용.
        // true면 알려진 6종에 없는, 실제 연결된 카메라가 보고한 추가 bias 행 - 연결 해제 시
        // 비활성화만 하는 게 아니라 아예 제거한다(false인 6종 고정 행은 항상 남아 있음).
        bool dynamic = false;
    };
    std::vector<BiasControlRow> m_biasRows;

    // 현재 m_language에 맞춰 row에 붙일 툴팁 문구를 고른다(한국어 번역이 없는 bias는 영어로 대체).
    QString BiasTooltipFor(const BiasControlRow& row) const;

    QRadioButton* m_radioLive = nullptr;
    QRadioButton* m_radioRaw = nullptr;
    QLineEdit* m_editRawPath = nullptr;
    QPushButton* m_btnBrowseRaw = nullptr;
    // Live 모드에서는 소스 박스가 카메라 식별자를 대신 보여주므로, RAW 모드로 돌아왔을 때
    // 복원할 수 있도록 마지막으로 고른 RAW 파일 경로를 따로 기억해 둔다.
    QString m_rawFilePath;
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
    QPushButton* m_btnStartStop = nullptr;
    QPushButton* m_btnPauseResume = nullptr;
    QPushButton* m_btnRecordSave = nullptr;
    QLabel* m_labelStateCaption = nullptr;
    QLabel* m_labelState = nullptr;
    QLabel* m_labelPreview = nullptr;
    QListWidget* m_listLog = nullptr;
    QSlider* m_sliderPosition = nullptr;
    QLabel* m_labelTime = nullptr;
};
