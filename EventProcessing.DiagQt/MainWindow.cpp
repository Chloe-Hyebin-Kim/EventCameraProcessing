#include "MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>

// CMakeLists.txt가 빌드 시점의 git 커밋 해시/날짜를 넣어 준다(EventProcessing.DiagQt/CMakeLists.txt
// 참고). 이 파일이 그 타깃 밖에서(예: 다른 빌드 스크립트로) 컴파일되는 경우를 대비한 기본값.
#ifndef EVENTCORE_GIT_COMMIT_HASH
#define EVENTCORE_GIT_COMMIT_HASH "unknown"
#endif
#ifndef EVENTCORE_BUILD_DATE
#define EVENTCORE_BUILD_DATE "unknown"
#endif

using namespace eventcore;
namespace fs = std::filesystem;

namespace
{
    QString FormatShotState(ShotState state)
    {
        switch (state)
        {
        case ShotState::Searching:  return QStringLiteral("SEARCHING");
        case ShotState::Ready:      return QStringLiteral("READY");
        case ShotState::Impact:     return QStringLiteral("IMPACT");
        case ShotState::Trajectory: return QStringLiteral("TRJCT");
        }
        return QStringLiteral("?");
    }

    // 슬라이더는 정수(int) 범위만 다루므로, 실제 마이크로초 타임스탬프 대신 [0, kSliderResolution]
    // 범위의 값으로 정규화해서 쓴다(긴 RAW 파일에서 타임스탬프가 int 범위를 넘는 것도 방지).
    constexpr int kSliderResolution = 10000;

    // 좌우 화살표 키 1회 입력당 이동하는 시간(1초).
    constexpr lli kArrowSeekStepUs = 1000000;

    // QString::toStdString()은 항상 UTF-8로 변환하지만, Windows의 파일 시스템 API와 Metavision
    // SDK는 시스템 코드페이지(예: 한글 Windows의 CP949)를 기대한다. 경로에 비ASCII 문자(한글
    // 폴더/파일명 등)가 있으면 UTF-8로 넘겼을 때 깨진 경로가 전달되어 파일을 못 열 수 있다
    // (MFC 버전에서 CT2A로 동일한 문제를 피했던 것과 같은 이유). toLocal8Bit()으로 시스템
    // 코드페이지에 맞게 변환한다(Linux 등에서는 보통 로케일이 이미 UTF-8이라 문제없음).
    std::string ToNativePath(const QString& path)
    {
        return path.toLocal8Bit().toStdString();
    }
}

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
{
    BuildUi();

    m_radioRaw->setChecked(true);
    m_editOutputDir->setText(QStringLiteral("./output"));
    m_editReadySec->setText(QStringLiteral("1.0"));
    m_editPreCaptureSec->setText(QStringLiteral("2.0"));
    m_editPostCaptureSec->setText(QStringLiteral("2.0"));
    m_editStablePx->setText(QStringLiteral("15"));
    m_editShotSpeed->setText(QStringLiteral("1000"));
    m_editDirConsistentFrames->setText(QStringLiteral("2"));
    m_editMaxDirDeviationDeg->setText(QStringLiteral("35"));
    m_editMissToleranceMs->setText(QStringLiteral("150"));
    m_editWindowUs->setText(QStringLiteral("10000"));

    UpdateRunButtons();
    m_labelState->setText(QStringLiteral("IDLE"));

    setWindowTitle(QStringLiteral("EventProcessing.DiagQt"));
    resize(960, 640);

    // RAW 파일이 끝까지 재생되면 LiveEventStream이 스스로 멈추는데(실시간 재생 스트림이라
    // 자연 종료를 UI에 알려줄 콜백이 없음), 그걸 놓치면 Start/Stop 버튼 상태가 계속
    // "재생 중"으로 남아 다음 Start 클릭이 씹히는 것처럼 보인다. 주기적으로 폴링해서 감지한다.
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &MainWindow::onPollStreamState);
    m_pollTimer->start(200);
}

MainWindow::~MainWindow()
{
    // 소멸 시 워커 스레드가 이미 파괴 중인 this를 건드리지 않도록 먼저 확실히 멈춘다.
    m_stream.Stop();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_stream.Stop();
    QWidget::closeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (m_running && m_seekRangeKnown && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right))
    {
        const lli currentUs = SliderValueToTimestamp(m_sliderPosition->value());
        const lli targetUs = (event->key() == Qt::Key_Left)
            ? (currentUs - kArrowSeekStepUs)
            : (currentUs + kArrowSeekStepUs);

        SeekTo(targetUs);
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void MainWindow::BuildUi()
{
    auto* root = new QVBoxLayout(this);

    // --- Menu bar: File - Settings - About ---
    // QWidget(비 QMainWindow)에도 QLayout::setMenuBar()로 메뉴바를 얹을 수 있다.
    auto* menuBar = new QMenuBar(this);

    // File: Source 그룹의 라디오/찾아보기 버튼과 똑같은 동작을 메뉴에서도 쓸 수 있게 한다
    // (버튼 자체는 그대로 남겨 둠 - 메뉴는 추가 경로일 뿐, 대체가 아님).
    m_menuFile = menuBar->addMenu(QString());
    m_menuMode = m_menuFile->addMenu(QString());

    m_actionModeLive = m_menuMode->addAction(QString());
    m_actionModeRaw = m_menuMode->addAction(QString());
    m_actionModeLive->setCheckable(true);
    m_actionModeRaw->setCheckable(true);

    auto* modeGroup = new QActionGroup(this);
    modeGroup->addAction(m_actionModeLive);
    modeGroup->addAction(m_actionModeRaw);

    m_menuFile->addSeparator();
    m_actionOpenFile = m_menuFile->addAction(QString());
    m_actionSetOutputPath = m_menuFile->addAction(QString());

    // Settings: English/Korean.
    m_menuSettings = menuBar->addMenu(QString());
    m_menuLanguage = m_menuSettings->addMenu(QString());

    m_actionLangEnglish = m_menuLanguage->addAction(QStringLiteral("English"));
    m_actionLangKorean = m_menuLanguage->addAction(QStringLiteral("한국어"));
    m_actionLangEnglish->setCheckable(true);
    m_actionLangKorean->setCheckable(true);
    m_actionLangEnglish->setChecked(true);

    auto* langGroup = new QActionGroup(this);
    langGroup->addAction(m_actionLangEnglish);
    langGroup->addAction(m_actionLangKorean);

    connect(m_actionLangEnglish, &QAction::triggered, this, [this]() { SetLanguage(AppLanguage::English); });
    connect(m_actionLangKorean, &QAction::triggered, this, [this]() { SetLanguage(AppLanguage::Korean); });

    // About: Version / License.
    m_menuAbout = menuBar->addMenu(QString());
    m_actionAboutVersion = m_menuAbout->addAction(QString());
    m_actionAboutLicense = m_menuAbout->addAction(QString());

    connect(m_actionAboutVersion, &QAction::triggered, this, &MainWindow::onShowAboutVersion);
    connect(m_actionAboutLicense, &QAction::triggered, this, &MainWindow::onShowAboutLicense);

    root->setMenuBar(menuBar);

    // --- Source ---
    m_boxSource = new QGroupBox(this);
    auto* sourceLayout = new QGridLayout(m_boxSource);

    m_radioLive = new QRadioButton(m_boxSource);
    m_radioRaw = new QRadioButton(m_boxSource);
    m_editRawPath = new QLineEdit(m_boxSource);
    m_btnBrowseRaw = new QPushButton(m_boxSource);

    sourceLayout->addWidget(m_radioLive, 0, 0);
    sourceLayout->addWidget(m_radioRaw, 0, 1);
    sourceLayout->addWidget(m_editRawPath, 1, 0, 1, 2);
    sourceLayout->addWidget(m_btnBrowseRaw, 1, 2);

    root->addWidget(m_boxSource);

    // --- Output ---
    m_boxOutput = new QGroupBox(this);
    auto* outputLayout = new QHBoxLayout(m_boxOutput);

    m_editOutputDir = new QLineEdit(m_boxOutput);
    m_btnBrowseOutput = new QPushButton(m_boxOutput);

    outputLayout->addWidget(m_editOutputDir);
    outputLayout->addWidget(m_btnBrowseOutput);

    root->addWidget(m_boxOutput);

    // --- Shot trigger params ---
    m_boxShotTrigger = new QGroupBox(this);
    auto* paramLayout = new QGridLayout(m_boxShotTrigger);

    m_editReadySec = new QLineEdit(m_boxShotTrigger);
    m_editPreCaptureSec = new QLineEdit(m_boxShotTrigger);
    m_editPostCaptureSec = new QLineEdit(m_boxShotTrigger);
    m_editStablePx = new QLineEdit(m_boxShotTrigger);
    m_editShotSpeed = new QLineEdit(m_boxShotTrigger);
    m_editDirConsistentFrames = new QLineEdit(m_boxShotTrigger);
    m_editMaxDirDeviationDeg = new QLineEdit(m_boxShotTrigger);
    m_editMissToleranceMs = new QLineEdit(m_boxShotTrigger);
    m_editWindowUs = new QLineEdit(m_boxShotTrigger);

    m_labelReadySec = new QLabel(m_boxShotTrigger);
    m_labelPreCaptureSec = new QLabel(m_boxShotTrigger);
    m_labelPostCaptureSec = new QLabel(m_boxShotTrigger);
    m_labelStablePx = new QLabel(m_boxShotTrigger);
    m_labelShotSpeed = new QLabel(m_boxShotTrigger);
    m_labelDirConsistentFrames = new QLabel(m_boxShotTrigger);
    m_labelMaxDirDeviationDeg = new QLabel(m_boxShotTrigger);
    m_labelMissToleranceMs = new QLabel(m_boxShotTrigger);
    m_labelWindowUs = new QLabel(m_boxShotTrigger);

    paramLayout->addWidget(m_labelReadySec, 0, 0);
    paramLayout->addWidget(m_editReadySec, 0, 1);
    paramLayout->addWidget(m_labelPreCaptureSec, 0, 2);
    paramLayout->addWidget(m_editPreCaptureSec, 0, 3);
    paramLayout->addWidget(m_labelPostCaptureSec, 0, 4);
    paramLayout->addWidget(m_editPostCaptureSec, 0, 5);
    paramLayout->addWidget(m_labelStablePx, 1, 0);
    paramLayout->addWidget(m_editStablePx, 1, 1);
    paramLayout->addWidget(m_labelShotSpeed, 1, 2);
    paramLayout->addWidget(m_editShotSpeed, 1, 3);
    paramLayout->addWidget(m_labelDirConsistentFrames, 1, 4);
    paramLayout->addWidget(m_editDirConsistentFrames, 1, 5);
    paramLayout->addWidget(m_labelMaxDirDeviationDeg, 2, 0);
    paramLayout->addWidget(m_editMaxDirDeviationDeg, 2, 1);
    paramLayout->addWidget(m_labelMissToleranceMs, 2, 2);
    paramLayout->addWidget(m_editMissToleranceMs, 2, 3);
    paramLayout->addWidget(m_labelWindowUs, 2, 4);
    paramLayout->addWidget(m_editWindowUs, 2, 5);

    root->addWidget(m_boxShotTrigger);

    // --- Controls ---
    auto* controlLayout = new QHBoxLayout();
    m_btnStartStop = new QPushButton(this);
    m_btnPauseResume = new QPushButton(this);
    m_labelStateCaption = new QLabel(this);
    m_labelState = new QLabel(QStringLiteral("IDLE"), this);
    m_labelState->setStyleSheet(QStringLiteral("font-weight: bold;"));

    controlLayout->addWidget(m_btnStartStop);
    controlLayout->addWidget(m_btnPauseResume);
    controlLayout->addStretch();
    controlLayout->addWidget(m_labelStateCaption);
    controlLayout->addWidget(m_labelState);

    root->addLayout(controlLayout);

    // --- Seek (RAW playback only; disabled/reset while stopped or on a live camera) ---
    auto* seekLayout = new QHBoxLayout();
    m_sliderPosition = new QSlider(Qt::Horizontal, this);
    m_sliderPosition->setRange(0, kSliderResolution);
    m_sliderPosition->setEnabled(false);
    m_labelTime = new QLabel(QStringLiteral("--:--.- / --:--.-"), this);

    seekLayout->addWidget(m_sliderPosition, 1);
    seekLayout->addWidget(m_labelTime);

    root->addLayout(seekLayout);

    // --- Preview + log ---
    auto* bodyLayout = new QHBoxLayout();

    m_labelPreview = new QLabel(this);
    m_labelPreview->setMinimumSize(480, 360);
    m_labelPreview->setAlignment(Qt::AlignCenter);
    m_labelPreview->setStyleSheet(QStringLiteral("background-color: black;"));
    m_labelPreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_listLog = new QListWidget(this);
    m_listLog->setMaximumWidth(320);

    bodyLayout->addWidget(m_labelPreview, 1);
    bodyLayout->addWidget(m_listLog);

    root->addLayout(bodyLayout, 1);

    connect(m_btnStartStop, &QPushButton::clicked, this, &MainWindow::onStartStopClicked);
    connect(m_btnPauseResume, &QPushButton::clicked, this, &MainWindow::onPauseResumeClicked);
    connect(m_btnBrowseRaw, &QPushButton::clicked, this, &MainWindow::onBrowseRawClicked);
    connect(m_btnBrowseOutput, &QPushButton::clicked, this, &MainWindow::onBrowseOutputClicked);
    connect(m_sliderPosition, &QSlider::sliderMoved, this, &MainWindow::onSliderMoved);
    connect(m_sliderPosition, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);

    // File > Mode와 Source 그룹의 라디오 버튼은 같은 선택을 나타내는 두 개의 창일 뿐이므로,
    // 어느 쪽을 눌러도 서로 맞춰지게 양방향으로 이어준다.
    connect(m_actionModeLive, &QAction::triggered, this, [this]() { m_radioLive->setChecked(true); });
    connect(m_actionModeRaw, &QAction::triggered, this, [this]() { m_radioRaw->setChecked(true); });
    connect(m_radioLive, &QRadioButton::toggled, this, [this](bool checked) { if (checked) m_actionModeLive->setChecked(true); });
    connect(m_radioRaw, &QRadioButton::toggled, this, [this](bool checked) { if (checked) m_actionModeRaw->setChecked(true); });

    // File > Open File / Set Output Path는 각각 기존 Browse... 버튼과 완전히 같은 동작을 한다.
    connect(m_actionOpenFile, &QAction::triggered, this, &MainWindow::onBrowseRawClicked);
    connect(m_actionSetOutputPath, &QAction::triggered, this, &MainWindow::onBrowseOutputClicked);

    RetranslateUi();
}

ShotTriggerConfig MainWindow::ReadConfigFromUI() const
{
    ShotTriggerConfig cfg;

    cfg.readySeconds = m_editReadySec->text().toDouble();
    cfg.preCaptureSeconds = m_editPreCaptureSec->text().toDouble();
    cfg.postCaptureSeconds = m_editPostCaptureSec->text().toDouble();
    cfg.stableMovePx = m_editStablePx->text().toFloat();
    cfg.shotSpeedPxPerSec = m_editShotSpeed->text().toFloat();
    cfg.directionConsistentFrames = std::max(1, m_editDirConsistentFrames->text().toInt());
    cfg.maxDirectionDeviationDeg = m_editMaxDirDeviationDeg->text().toFloat();
    cfg.missToleranceUs = static_cast<lli>(m_editMissToleranceMs->text().toDouble() * 1000.0);

    return cfg;
}

void MainWindow::AppendLog(const QString& msg)
{
    const QString line = QStringLiteral("[%1] %2")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(msg);

    m_listLog->addItem(line);
    m_listLog->scrollToBottom();
}

void MainWindow::UpdateStateLabel(ShotState state)
{
    m_labelState->setText(FormatShotState(state));
}

void MainWindow::UpdateRunButtons()
{
    switch (m_runState)
    {
    case RunState::Idle:
        m_btnStartStop->setText(Tr(QStringLiteral("Start"), QStringLiteral("시작")));
        m_btnStartStop->setEnabled(true);
        m_btnPauseResume->setText(Tr(QStringLiteral("Pause"), QStringLiteral("멈춤")));
        m_btnPauseResume->setEnabled(false);
        break;
    case RunState::Running:
        m_btnStartStop->setText(Tr(QStringLiteral("Stop"), QStringLiteral("중단")));
        m_btnStartStop->setEnabled(true);
        m_btnPauseResume->setText(Tr(QStringLiteral("Pause"), QStringLiteral("멈춤")));
        m_btnPauseResume->setEnabled(true);
        break;
    case RunState::Paused:
        m_btnStartStop->setText(Tr(QStringLiteral("Stop"), QStringLiteral("중단")));
        m_btnStartStop->setEnabled(true);
        m_btnPauseResume->setText(Tr(QStringLiteral("Resume"), QStringLiteral("재개")));
        m_btnPauseResume->setEnabled(true);
        break;
    }
}

QString MainWindow::Tr(const QString& en, const QString& ko) const
{
    return m_language == AppLanguage::Korean ? ko : en;
}

void MainWindow::SetLanguage(AppLanguage lang)
{
    if (m_language == lang)
    {
        return;
    }

    m_language = lang;
    RetranslateUi();
}

void MainWindow::RetranslateUi()
{
    setWindowTitle(QStringLiteral("EventProcessing.DiagQt"));

    m_menuFile->setTitle(Tr(QStringLiteral("File"), QStringLiteral("파일")));
    m_menuMode->setTitle(Tr(QStringLiteral("Mode"), QStringLiteral("모드")));
    m_actionModeLive->setText(Tr(QStringLiteral("Live camera"), QStringLiteral("라이브 카메라")));
    m_actionModeRaw->setText(Tr(QStringLiteral("RAW file"), QStringLiteral("RAW 파일")));
    m_actionOpenFile->setText(Tr(QStringLiteral("Open File..."), QStringLiteral("파일 열기...")));
    m_actionSetOutputPath->setText(Tr(QStringLiteral("Set Output Path..."), QStringLiteral("산출물 경로 설정...")));

    m_menuSettings->setTitle(Tr(QStringLiteral("Settings"), QStringLiteral("설정")));
    m_menuLanguage->setTitle(Tr(QStringLiteral("Language"), QStringLiteral("언어")));
    // 언어 이름 자체(English/한국어)는 관례상 항상 그 언어로 표시하고 번역하지 않는다.

    m_menuAbout->setTitle(Tr(QStringLiteral("About"), QStringLiteral("정보")));
    m_actionAboutVersion->setText(Tr(QStringLiteral("Version"), QStringLiteral("버전")));
    m_actionAboutLicense->setText(Tr(QStringLiteral("License"), QStringLiteral("라이센스")));

    m_boxSource->setTitle(Tr(QStringLiteral("Source"), QStringLiteral("입력 소스")));
    m_radioLive->setText(Tr(QStringLiteral("Live camera"), QStringLiteral("라이브 카메라")));
    m_radioRaw->setText(Tr(QStringLiteral("RAW file"), QStringLiteral("RAW 파일")));
    m_btnBrowseRaw->setText(Tr(QStringLiteral("Browse..."), QStringLiteral("찾아보기...")));

    m_boxOutput->setTitle(Tr(QStringLiteral("Output"), QStringLiteral("출력")));
    m_btnBrowseOutput->setText(Tr(QStringLiteral("Browse..."), QStringLiteral("찾아보기...")));

    m_boxShotTrigger->setTitle(Tr(QStringLiteral("Shot Trigger"), QStringLiteral("샷 트리거")));

    m_labelReadySec->setText(Tr(QStringLiteral("Ready (sec)"), QStringLiteral("정지 유지 시간 (초)")));
    m_labelPreCaptureSec->setText(Tr(QStringLiteral("Pre-capture (sec)"), QStringLiteral("사전 저장 시간 (초)")));
    m_labelPostCaptureSec->setText(Tr(QStringLiteral("Post-capture (sec)"), QStringLiteral("사후 저장 시간 (초)")));
    m_labelStablePx->setText(Tr(QStringLiteral("Stable move (px)"), QStringLiteral("허용 흔들림 (px)")));
    m_labelShotSpeed->setText(Tr(QStringLiteral("Shot speed (px/s)"), QStringLiteral("샷 속도 (px/s)")));
    m_labelDirConsistentFrames->setText(Tr(QStringLiteral("Direction consistent frames"), QStringLiteral("방향 일관성 확인 구간 수")));
    m_labelMaxDirDeviationDeg->setText(Tr(QStringLiteral("Max direction deviation (deg)"), QStringLiteral("최대 방향 편차 (도)")));
    m_labelMissToleranceMs->setText(Tr(QStringLiteral("Miss tolerance (ms)"), QStringLiteral("검출 유실 허용 시간 (ms)")));
    m_labelWindowUs->setText(Tr(QStringLiteral("Window (us)"), QStringLiteral("처리 윈도우 (us)")));

    m_editReadySec->setToolTip(Tr(
        QStringLiteral(
            "How long (seconds) the ball must stay still at the same spot\n"
            "before the state machine enters READY."),
        QStringLiteral(
            "공이 같은 위치에서 이 시간(초) 이상 멈춰 있어야\n"
            "READY 상태로 전환됩니다.")));
    m_editPreCaptureSec->setToolTip(Tr(
        QStringLiteral(
            "How many seconds of buffered frames BEFORE the Impact frame\n"
            "to include when saving the Trajectory (TRJCT) capture."),
        QStringLiteral(
            "Impact 프레임 이전 몇 초 분량의 버퍼링된 프레임을\n"
            "Trajectory(TRJCT) 저장에 포함할지 지정합니다.")));
    m_editPostCaptureSec->setToolTip(Tr(
        QStringLiteral(
            "How many seconds AFTER the Impact frame to keep saving frames\n"
            "during Trajectory (TRJCT), before returning to SEARCHING."),
        QStringLiteral(
            "Impact 프레임 이후 몇 초 동안 Trajectory(TRJCT) 상태로\n"
            "프레임을 계속 저장한 뒤 SEARCHING으로 복귀할지 지정합니다.")));
    m_editStablePx->setToolTip(Tr(
        QStringLiteral(
            "Maximum center-point jitter (pixels) still counted as \"stationary\"\n"
            "while in READY. Also the threshold used to detect the first frame\n"
            "the ball leaves that spot (the start of a possible shot)."),
        QStringLiteral(
            "READY 상태에서 \"정지\"로 인정할 최대 중심점 흔들림(픽셀)입니다.\n"
            "또한 공이 그 위치를 벗어난 첫 프레임(샷 시작 후보)을\n"
            "판단하는 기준이기도 합니다.")));
    m_editShotSpeed->setToolTip(Tr(
        QStringLiteral(
            "Minimum center-point speed (pixels/second) a movement must reach,\n"
            "in addition to direction consistency, to count toward confirming\n"
            "Impact."),
        QStringLiteral(
            "방향 일관성과 별개로, 이동이 Impact 확정에 반영되려면\n"
            "최소한 이 이상의 중심점 이동 속도(픽셀/초)가 필요합니다.")));
    m_editDirConsistentFrames->setToolTip(Tr(
        QStringLiteral(
            "How many consecutive direction comparisons must stay within\n"
            "\"Max direction deviation\" before a movement is confirmed as a real\n"
            "shot (Impact) rather than zigzag noise."),
        QStringLiteral(
            "잡음/지그재그가 아니라 실제 샷(Impact)으로 확정하려면,\n"
            "연속된 방향 비교가 \"최대 방향 편차\" 이내로 몇 구간\n"
            "연속돼야 하는지를 지정합니다.")));
    m_editMaxDirDeviationDeg->setToolTip(Tr(
        QStringLiteral(
            "Maximum angle (degrees) allowed between consecutive movement\n"
            "vectors while confirming a shot. A bigger change is treated as\n"
            "zigzag/noise, which resets back to SEARCHING."),
        QStringLiteral(
            "샷을 확정하는 동안 연속된 이동 벡터 사이에 허용하는\n"
            "최대 각도(도)입니다. 이보다 크게 꺾이면 지그재그/잡음으로\n"
            "간주해 SEARCHING으로 리셋됩니다.")));
    m_editMissToleranceMs->setToolTip(Tr(
        QStringLiteral(
            "How long (milliseconds) a brief ball-detection dropout is tolerated\n"
            "without resetting the state. A stationary ball produces very few\n"
            "events, so detection can blink out for a moment even though the\n"
            "ball hasn't actually moved."),
        QStringLiteral(
            "짧은 공 검출 유실을 상태 리셋 없이 허용하는 시간(밀리초)입니다.\n"
            "정지된 공은 이벤트가 거의 없어 실제로는 움직이지 않았어도\n"
            "한순간 검출이 끊길 수 있습니다.")));
    m_editWindowUs->setToolTip(Tr(
        QStringLiteral(
            "Event-accumulation window length (microseconds): how often a new\n"
            "frame is built and the ball position is re-evaluated."),
        QStringLiteral(
            "이벤트 누적 윈도우 길이(마이크로초): 새 프레임을 만들고\n"
            "공 위치를 다시 평가하는 주기입니다.")));

    m_labelStateCaption->setText(Tr(QStringLiteral("State:"), QStringLiteral("상태:")));

    UpdateRunButtons();
}

void MainWindow::DrawFrame(const cv::Mat& bgrFrame)
{
    if (bgrFrame.empty() || bgrFrame.type() != CV_8UC3 || m_labelPreview == nullptr)
    {
        return;
    }

    const cv::Mat safe = bgrFrame.isContinuous() ? bgrFrame : bgrFrame.clone();

    const QImage image(safe.data, safe.cols, safe.rows, static_cast<int>(safe.step), QImage::Format_BGR888);

    m_previewPixmap = QPixmap::fromImage(image);
    m_labelPreview->setPixmap(m_previewPixmap.scaled(
        m_labelPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

lli MainWindow::SliderValueToTimestamp(int value) const
{
    if (!m_seekRangeKnown || m_seekEndUs <= m_seekStartUs)
    {
        return m_seekStartUs;
    }

    const double t = static_cast<double>(value) / static_cast<double>(kSliderResolution);
    return m_seekStartUs + static_cast<lli>(t * static_cast<double>(m_seekEndUs - m_seekStartUs));
}

int MainWindow::TimestampToSliderValue(lli timestampUs) const
{
    if (!m_seekRangeKnown || m_seekEndUs <= m_seekStartUs)
    {
        return 0;
    }

    const double t = static_cast<double>(timestampUs - m_seekStartUs) / static_cast<double>(m_seekEndUs - m_seekStartUs);
    return static_cast<int>(std::clamp(t, 0.0, 1.0) * kSliderResolution);
}

QString MainWindow::FormatTimeUs(lli us)
{
    if (us < 0)
    {
        us = 0;
    }

    const double totalSeconds = static_cast<double>(us) / 1000000.0;
    const int minutes = static_cast<int>(totalSeconds) / 60;
    const double seconds = totalSeconds - minutes * 60;

    return QStringLiteral("%1:%2")
        .arg(minutes)
        .arg(seconds, 4, 'f', 1, QChar('0'));
}

void MainWindow::UpdateTimeLabel(lli currentUs)
{
    if (!m_seekRangeKnown)
    {
        m_labelTime->setText(FormatTimeUs(currentUs) + QStringLiteral(" / --:--.-"));
        return;
    }

    const lli durationUs = m_seekEndUs - m_seekStartUs;
    const lli relativeUs = std::clamp(currentUs - m_seekStartUs, static_cast<lli>(0), durationUs);

    m_labelTime->setText(FormatTimeUs(relativeUs) + QStringLiteral(" / ") + FormatTimeUs(durationUs));
}

void MainWindow::SeekTo(lli timestampUs)
{
    if (!m_running || !m_seekRangeKnown)
    {
        return;
    }

    const lli clamped = std::clamp(timestampUs, m_seekStartUs, m_seekEndUs);

    if (!m_stream.Seek(clamped))
    {
        AppendLog(QStringLiteral("Seek failed"));
        return;
    }

    m_sliderPosition->blockSignals(true);
    m_sliderPosition->setValue(TimestampToSliderValue(clamped));
    m_sliderPosition->blockSignals(false);
    UpdateTimeLabel(clamped);
}

void MainWindow::StartCaptureSave()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QString folder = QStringLiteral("%1/shot_%2")
        .arg(m_outputDir)
        .arg(now.toString(QStringLiteral("yyyyMMdd_HHmmss")));

    std::error_code ec;
    fs::create_directories(ToNativePath(m_outputDir), ec);
    fs::create_directories(ToNativePath(folder), ec);

    m_currentCaptureDir = folder;
    m_captureFrameIndex = 0;
    m_capturingNow = true;
}

void MainWindow::SaveCaptureFrame(const cv::Mat& bgrFrame)
{
    if (!m_capturingNow || bgrFrame.empty())
    {
        return;
    }

    const QString filename = QStringLiteral("%1/frame_%2.png")
        .arg(m_currentCaptureDir)
        .arg(m_captureFrameIndex, 4, 10, QChar('0'));

    cv::imwrite(ToNativePath(filename), bgrFrame);

    ++m_captureFrameIndex;
}

void MainWindow::FinishCaptureSave()
{
    m_capturingNow = false;
}

void MainWindow::PushPreRollFrame(const std::shared_ptr<FrameMessage>& msg)
{
    m_preRollBuffer.push_back(msg);

    const lli preRollUs = static_cast<lli>(m_activeConfig.preCaptureSeconds * 1000000.0);

    while (!m_preRollBuffer.empty()
        && (msg->windowStartUs - m_preRollBuffer.front()->windowStartUs) > preRollUs)
    {
        m_preRollBuffer.pop_front();
    }
}

void MainWindow::FlushPreRollBuffer(lli impactUs)
{
    const lli preRollStartUs = impactUs - static_cast<lli>(m_activeConfig.preCaptureSeconds * 1000000.0);

    int savedCount = 0;

    for (const auto& buffered : m_preRollBuffer)
    {
        if (buffered->windowStartUs >= preRollStartUs)
        {
            SaveCaptureFrame(buffered->frame);
            ++savedCount;
        }
    }

    m_preRollBuffer.clear();

    AppendLog(QStringLiteral("IMPACT - trajectory capture started (%1 pre-roll frame(s))").arg(savedCount));
}

void MainWindow::onStartStopClicked()
{
    if (m_runState == RunState::Idle)
    {
        StartStream();
    }
    else
    {
        StopStream(QStringLiteral("Stopped"));
    }
}

void MainWindow::onPauseResumeClicked()
{
    switch (m_runState)
    {
    case RunState::Running:
        PauseStream();
        break;
    case RunState::Paused:
        ResumeStream();
        break;
    case RunState::Idle:
        break;
    }
}

void MainWindow::StartStream()
{
    if (m_runState != RunState::Idle)
    {
        return;
    }

    const QString rawPath = m_editRawPath->text();
    m_outputDir = m_editOutputDir->text();

    std::error_code ec;
    fs::create_directories(ToNativePath(m_outputDir), ec);

    lli windowUs = m_editWindowUs->text().toLongLong();
    if (windowUs <= 0)
    {
        windowUs = 10000;
    }

    m_activeConfig = ReadConfigFromUI();
    m_trigger = ShotTrigger(m_activeConfig);
    m_capturingNow = false;
    m_captureFrameIndex = 0;
    m_preRollBuffer.clear();

    m_gotFirstFrame = false;
    m_seekRangeKnown = false;
    m_seekStartUs = 0;
    m_seekEndUs = 0;
    m_sliderPosition->setEnabled(false);
    m_sliderPosition->setValue(0);
    m_labelTime->setText(QStringLiteral("--:--.- / --:--.-"));

    const bool live = m_radioLive->isChecked();
    const std::string rawPathStd = ToNativePath(rawPath);

    if (!live && rawPathStd.empty())
    {
        AppendLog(QStringLiteral("Please choose a RAW file, or select 'Live camera'."));
        return;
    }

    // 콜백은 워커 스레드에서 호출된다. this를 직접 캡처해 호출하는 대신, QMetaObject::invokeMethod의
    // context-object 오버로드를 사용해 UI 스레드로 안전하게 마샬링한다 (this가 이미 파괴되었다면
    // Qt가 알아서 호출을 건너뛴다).
    const bool ok = m_stream.Start(
        live ? "" : rawPathStd.c_str(),
        windowUs,
        [this](const EventProcessingResult& result, lli startUs, lli endUs)
        {
            auto msg = std::make_shared<FrameMessage>();
            msg->frame = result.debugImage.clone();
            msg->ball = result.ball;
            msg->windowStartUs = startUs;
            msg->windowEndUs = endUs;

            QMetaObject::invokeMethod(this, [this, msg]() { OnFrameReady(msg); }, Qt::QueuedConnection);
        });

    if (!ok)
    {
        QString msg = QStringLiteral("Failed to start stream");
        const std::string& err = m_stream.LastError();
        if (!err.empty())
        {
            msg += QStringLiteral(": ") + QString::fromStdString(err);
        }
        AppendLog(msg);
        return;
    }

    m_running = true;
    m_liveMode = live;
    m_processingPaused = false;
    m_runState = RunState::Running;
    UpdateRunButtons();
    m_labelState->setText(QStringLiteral("SEARCHING"));
    AppendLog(live
        ? QStringLiteral("Started (live camera) - recording")
        : QStringLiteral("Started (RAW playback)"));
}

void MainWindow::PauseStream()
{
    if (m_runState != RunState::Running)
    {
        return;
    }

    if (m_liveMode)
    {
        // 카메라/미리보기는 그대로 흐르게 둔다(끼어든 상황이 지나가는 걸 볼 수 있도록). ShotTrigger
        // 갱신과 프레임 저장(녹화)만 건너뛴다 - OnFrameReady에서 m_processingPaused를 확인해 처리.
        m_processingPaused = true;
        AppendLog(QStringLiteral("PAUSED - live preview continues, recording suspended"));
    }
    else
    {
        // RAW 재생 자체를 멈춰서(카메라 정지) 더 이상 새 프레임이 오지 않게 한다 -> 화면이
        // 멈춘 그 자리에 그대로 남는다.
        if (!m_stream.Pause())
        {
            AppendLog(QStringLiteral("Pause failed"));
            return;
        }
        m_processingPaused = true;
        AppendLog(QStringLiteral("PAUSED - playback frozen"));
    }

    m_runState = RunState::Paused;
    UpdateRunButtons();
}

void MainWindow::ResumeStream()
{
    if (m_runState != RunState::Paused)
    {
        return;
    }

    if (!m_liveMode)
    {
        if (!m_stream.Resume())
        {
            AppendLog(QStringLiteral("Resume failed"));
            return;
        }
    }

    m_processingPaused = false;
    m_runState = RunState::Running;
    UpdateRunButtons();
    AppendLog(m_liveMode
        ? QStringLiteral("RESUMED - recording")
        : QStringLiteral("RESUMED - playback"));
}

void MainWindow::onPollStreamState()
{
    if (!m_running)
    {
        return;
    }

    if (!m_stream.IsRunning())
    {
        // RAW 파일이 끝까지 재생되어 LiveEventStream이 스스로 멈춘 경우. m_stream.Stop()은
        // 이미 멈춘 스트림에 대해서도 안전하게 호출할 수 있고(워커 스레드 join 보장),
        // Stop 버튼을 누른 것과 동일하게 UI 상태를 정리한다.
        StopStream(QStringLiteral("Playback finished (reached end of RAW file)"));
        return;
    }

    if (!m_seekRangeKnown && m_gotFirstFrame)
    {
        // 실제 프레임을 받기 전에는 카메라가 아직 완전히 준비되지 않았을 수 있어 여기로 오지
        // 않는다(위 m_gotFirstFrame 조건). 그 뒤에도 SDK가 탐색 범위를 못 정했을 수 있으므로
        // (Live 카메라라면 계속 실패함), 준비될 때까지 매 폴링마다 다시 시도한다.
        if (m_stream.GetSeekRange(m_seekStartUs, m_seekEndUs) && m_seekEndUs > m_seekStartUs)
        {
            m_seekRangeKnown = true;
            m_sliderPosition->setEnabled(true);
        }
    }
}

void MainWindow::StopStream(const QString& logMessage)
{
    m_stream.Stop();
    m_running = false;
    m_processingPaused = false;
    m_runState = RunState::Idle;
    UpdateRunButtons();

    m_labelState->setText(QStringLiteral("IDLE"));
    m_gotFirstFrame = false;
    m_seekRangeKnown = false;
    m_sliderPosition->setEnabled(false);
    m_sliderPosition->setValue(0);
    m_labelTime->setText(QStringLiteral("--:--.- / --:--.-"));
    m_preRollBuffer.clear();

    // 처음(까만 화면)으로 되돌린다.
    m_previewPixmap = QPixmap();
    m_labelPreview->setPixmap(QPixmap());

    AppendLog(logMessage);
}

void MainWindow::onBrowseRawClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        Tr(QStringLiteral("Select RAW file"), QStringLiteral("RAW 파일 선택")),
        QString(),
        Tr(QStringLiteral("Metavision RAW (*.raw);;All Files (*)"), QStringLiteral("Metavision RAW (*.raw);;모든 파일 (*)")));

    if (!path.isEmpty())
    {
        m_editRawPath->setText(path);
        m_radioRaw->setChecked(true);
    }
}

void MainWindow::onBrowseOutputClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(this, Tr(QStringLiteral("Select output folder"), QStringLiteral("출력 폴더 선택")));

    if (!dir.isEmpty())
    {
        m_editOutputDir->setText(dir);
    }
}

void MainWindow::onShowAboutVersion()
{
    // 정식 버전 번호 체계가 아직 없어서, 실제로 검증 가능한 값인 빌드 시점의 git 커밋 해시와
    // 날짜를 대신 보여준다(EventProcessing.DiagQt/CMakeLists.txt에서 컴파일 시 주입).
    const QString text = Tr(QStringLiteral("Build: %1 (%2)"), QStringLiteral("빌드: %1 (%2)"))
        .arg(QStringLiteral(EVENTCORE_GIT_COMMIT_HASH))
        .arg(QStringLiteral(EVENTCORE_BUILD_DATE));

    QMessageBox::information(this, Tr(QStringLiteral("Version"), QStringLiteral("버전")), text);
}

void MainWindow::onShowAboutLicense()
{
    // 이 블록은 의도적으로 언어 설정과 무관하게 항상 영어 원문 그대로 보여준다(로그와 같은 이유 -
    // 법적/연락처 정보라 의역하지 않음). 정식 라이선스가 아직 지정되지 않았고 저장소에 LICENSE
    // 파일이 없으므로, 없는 사실(라이선스 종류, 웹사이트 등)을 지어내는 대신 실제로 확인 가능한
    // 내용만 채운다: 번들된 서드파티 SDK의 실제 라이선스 파일 경로, 실제 GitHub 저장소 주소 등.
    const QString text = QStringLiteral(
        "======================================================================\n"
        " PRODUCT NAME : Event Camera Processor\n"
        " VERSION      : Build %1 (%2)\n"
        " COPYRIGHT    : Copyright (c) %3 HyeBin Kim, Graduate School of\n"
        "                Engineering Practice, Seoul National University.\n"
        "                All rights reserved.\n"
        "======================================================================\n"
        "\n"
        " LICENSING TERMS :\n"
        " No open-source license has been assigned to this software yet\n"
        " (this repository does not currently contain a LICENSE file). All\n"
        " rights are reserved by the copyright holder above unless otherwise\n"
        " agreed in writing.\n"
        "\n"
        " THIRD-PARTY LICENSES :\n"
        " This application uses the following third-party components:\n"
        "   - Prophesee Metavision SDK\n"
        "     see Prophesee/share/metavision/licensing/LICENSE_OPEN\n"
        "   - HDF5 (ECF codec, bundled with the Metavision SDK)\n"
        "     see Prophesee/share/hdf5_ecf/LICENSE\n"
        "   - OpenCV 4.4.x (bundled under ocv440/)\n"
        "     BSD 3-Clause License (per OpenCV's own release notes; no\n"
        "     LICENSE file is bundled in ocv440/ to check directly)\n"
        "   - Qt Widgets (not bundled; found on your system via CMake)\n"
        "     LGPLv3 / GPLv3, or commercial, depending on your Qt install\n"
        "\n"
        " CONTACT / SUPPORT :\n"
        " - Email      : dev5igner@snu.ac.kr\n"
        " - Tel        : +82-010-2008-2026\n"
        " - Address    : 1 Gwanak-ro, Gwanak-gu, Seoul, Republic of Korea\n"
        " - Website    : https://github.com/Chloe-Hyebin-Kim/EventCameraProcessing\n"
        " - Bug Report : https://github.com/Chloe-Hyebin-Kim/EventCameraProcessing/issues\n"
        "======================================================================")
        .arg(QStringLiteral(EVENTCORE_GIT_COMMIT_HASH))
        .arg(QStringLiteral(EVENTCORE_BUILD_DATE))
        .arg(QDate::currentDate().year());

    QMessageBox::information(this, Tr(QStringLiteral("License"), QStringLiteral("라이센스")), text);
}

void MainWindow::onSliderMoved(int value)
{
    // 드래그 중에는 미리보기로 시간만 갱신하고, 실제 탐색은 손을 뗄 때(onSliderReleased) 한다.
    UpdateTimeLabel(SliderValueToTimestamp(value));
}

void MainWindow::onSliderReleased()
{
    if (!m_running || !m_seekRangeKnown)
    {
        return;
    }

    SeekTo(SliderValueToTimestamp(m_sliderPosition->value()));
}

void MainWindow::OnFrameReady(std::shared_ptr<FrameMessage> msg)
{
    if (!msg || msg->frame.empty())
    {
        return;
    }

    m_gotFirstFrame = true;

    DrawFrame(msg->frame);

    if (m_processingPaused)
    {
        // Live 모드는 카메라를 세우지 않으므로 pause 중에도 이 콜백이 계속 들어온다 - 화면은
        // 이미 위에서 갱신했으니(끼어든 상황을 볼 수 있게), 트리거 갱신/저장(녹화)만 건너뛴다.
        // RAW 모드는 m_stream.Pause()가 카메라 자체를 세워서 보통 여기로 오지도 않지만, pause
        // 호출과 경합하며 이미 큐에 들어와 있던 콜백이 뒤늦게 도착하는 경우를 대비한 방어.
        return;
    }

    // 사용자가 슬라이더를 드래그하는 중에는 재생 위치가 그 값을 덮어쓰지 않도록 한다.
    if (m_seekRangeKnown && !m_sliderPosition->isSliderDown())
    {
        m_sliderPosition->blockSignals(true);
        m_sliderPosition->setValue(TimestampToSliderValue(msg->windowStartUs));
        m_sliderPosition->blockSignals(false);
    }
    UpdateTimeLabel(msg->windowStartUs);

    const ShotUpdateResult su = m_trigger.Update(msg->ball, msg->windowStartUs);
    UpdateStateLabel(su.state);

    // Trajectory 상태(TRJCT)에 들어가기 전까지의 모든 프레임은 Impact가 언제 확정될지 몰라도
    // 미리 링 버퍼에 쌓아 둔다. Impact가 확정되면 이 버퍼에서 preCaptureSeconds 분량을 저장한다.
    if (su.state != ShotState::Trajectory)
    {
        PushPreRollFrame(msg);
    }

    if (su.justEnteredReady)
    {
        AppendLog(QStringLiteral("READY"));
    }

    if (su.justTriggered)
    {
        StartCaptureSave();
        FlushPreRollBuffer(m_trigger.TriggerTimeUs());
    }

    if (su.state == ShotState::Trajectory)
    {
        SaveCaptureFrame(msg->frame);
    }

    if (su.justFinishedTrajectory)
    {
        AppendLog(QStringLiteral("Trajectory capture finished: %1 frame(s) saved to %2")
            .arg(m_captureFrameIndex)
            .arg(m_currentCaptureDir));
        FinishCaptureSave();
    }
}
