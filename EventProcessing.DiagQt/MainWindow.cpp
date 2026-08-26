#include "MainWindow.h"

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
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>

using namespace eventcore;
namespace fs = std::filesystem;

namespace
{
    QString FormatShotState(ShotState state)
    {
        switch (state)
        {
        case ShotState::Searching: return QStringLiteral("SEARCHING");
        case ShotState::Ready:     return QStringLiteral("READY");
        case ShotState::Capturing: return QStringLiteral("CAPTURING");
        }
        return QStringLiteral("?");
    }

    // 슬라이더는 정수(int) 범위만 다루므로, 실제 마이크로초 타임스탬프 대신 [0, kSliderResolution]
    // 범위의 값으로 정규화해서 쓴다(긴 RAW 파일에서 타임스탬프가 int 범위를 넘는 것도 방지).
    constexpr int kSliderResolution = 10000;

    // 좌우 화살표 키 1회 입력당 이동하는 시간(1초).
    constexpr lli kArrowSeekStepUs = 1000000;
}

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
{
    BuildUi();

    m_radioRaw->setChecked(true);
    m_editOutputDir->setText(QStringLiteral("./output"));
    m_editReadySec->setText(QStringLiteral("1.0"));
    m_editCaptureSec->setText(QStringLiteral("1.0"));
    m_editStablePx->setText(QStringLiteral("15"));
    m_editShotSpeed->setText(QStringLiteral("1000"));
    m_editMissToleranceMs->setText(QStringLiteral("150"));
    m_editWindowUs->setText(QStringLiteral("10000"));

    m_btnStop->setEnabled(false);
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

    // --- Source ---
    auto* sourceBox = new QGroupBox(QStringLiteral("Source"), this);
    auto* sourceLayout = new QGridLayout(sourceBox);

    m_radioLive = new QRadioButton(QStringLiteral("Live camera"), sourceBox);
    m_radioRaw = new QRadioButton(QStringLiteral("RAW file"), sourceBox);
    m_editRawPath = new QLineEdit(sourceBox);
    m_btnBrowseRaw = new QPushButton(QStringLiteral("Browse..."), sourceBox);

    sourceLayout->addWidget(m_radioLive, 0, 0);
    sourceLayout->addWidget(m_radioRaw, 0, 1);
    sourceLayout->addWidget(m_editRawPath, 1, 0, 1, 2);
    sourceLayout->addWidget(m_btnBrowseRaw, 1, 2);

    root->addWidget(sourceBox);

    // --- Output ---
    auto* outputBox = new QGroupBox(QStringLiteral("Output"), this);
    auto* outputLayout = new QHBoxLayout(outputBox);

    m_editOutputDir = new QLineEdit(outputBox);
    m_btnBrowseOutput = new QPushButton(QStringLiteral("Browse..."), outputBox);

    outputLayout->addWidget(m_editOutputDir);
    outputLayout->addWidget(m_btnBrowseOutput);

    root->addWidget(outputBox);

    // --- Shot trigger params ---
    auto* paramBox = new QGroupBox(QStringLiteral("Shot Trigger"), this);
    auto* paramLayout = new QGridLayout(paramBox);

    m_editReadySec = new QLineEdit(paramBox);
    m_editCaptureSec = new QLineEdit(paramBox);
    m_editStablePx = new QLineEdit(paramBox);
    m_editShotSpeed = new QLineEdit(paramBox);
    m_editMissToleranceMs = new QLineEdit(paramBox);
    m_editWindowUs = new QLineEdit(paramBox);

    paramLayout->addWidget(new QLabel(QStringLiteral("Ready (sec)")), 0, 0);
    paramLayout->addWidget(m_editReadySec, 0, 1);
    paramLayout->addWidget(new QLabel(QStringLiteral("Capture (sec)")), 0, 2);
    paramLayout->addWidget(m_editCaptureSec, 0, 3);
    paramLayout->addWidget(new QLabel(QStringLiteral("Stable move (px)")), 1, 0);
    paramLayout->addWidget(m_editStablePx, 1, 1);
    paramLayout->addWidget(new QLabel(QStringLiteral("Shot speed (px/s)")), 1, 2);
    paramLayout->addWidget(m_editShotSpeed, 1, 3);
    paramLayout->addWidget(new QLabel(QStringLiteral("Miss tolerance (ms)")), 2, 0);
    paramLayout->addWidget(m_editMissToleranceMs, 2, 1);
    paramLayout->addWidget(new QLabel(QStringLiteral("Window (us)")), 2, 2);
    paramLayout->addWidget(m_editWindowUs, 2, 3);

    root->addWidget(paramBox);

    // --- Controls ---
    auto* controlLayout = new QHBoxLayout();
    m_btnStart = new QPushButton(QStringLiteral("Start"), this);
    m_btnStop = new QPushButton(QStringLiteral("Stop"), this);
    m_labelState = new QLabel(QStringLiteral("IDLE"), this);
    m_labelState->setStyleSheet(QStringLiteral("font-weight: bold;"));

    controlLayout->addWidget(m_btnStart);
    controlLayout->addWidget(m_btnStop);
    controlLayout->addStretch();
    controlLayout->addWidget(new QLabel(QStringLiteral("State:"), this));
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

    connect(m_btnStart, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_btnStop, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_btnBrowseRaw, &QPushButton::clicked, this, &MainWindow::onBrowseRawClicked);
    connect(m_btnBrowseOutput, &QPushButton::clicked, this, &MainWindow::onBrowseOutputClicked);
    connect(m_sliderPosition, &QSlider::sliderMoved, this, &MainWindow::onSliderMoved);
    connect(m_sliderPosition, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);
}

ShotTriggerConfig MainWindow::ReadConfigFromUI() const
{
    ShotTriggerConfig cfg;

    cfg.readySeconds = m_editReadySec->text().toDouble();
    cfg.captureSeconds = m_editCaptureSec->text().toDouble();
    cfg.stableMovePx = m_editStablePx->text().toFloat();
    cfg.shotSpeedPxPerSec = m_editShotSpeed->text().toFloat();
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
    fs::create_directories(m_outputDir.toStdString(), ec);
    fs::create_directories(folder.toStdString(), ec);

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

    cv::imwrite(filename.toStdString(), bgrFrame);

    ++m_captureFrameIndex;
}

void MainWindow::FinishCaptureSave()
{
    m_capturingNow = false;
}

void MainWindow::onStartClicked()
{
    if (m_running)
    {
        return;
    }

    const QString rawPath = m_editRawPath->text();
    m_outputDir = m_editOutputDir->text();

    std::error_code ec;
    fs::create_directories(m_outputDir.toStdString(), ec);

    lli windowUs = m_editWindowUs->text().toLongLong();
    if (windowUs <= 0)
    {
        windowUs = 10000;
    }

    m_trigger = ShotTrigger(ReadConfigFromUI());
    m_capturingNow = false;
    m_captureFrameIndex = 0;

    m_seekRangeKnown = false;
    m_seekStartUs = 0;
    m_seekEndUs = 0;
    m_sliderPosition->setEnabled(false);
    m_sliderPosition->setValue(0);
    m_labelTime->setText(QStringLiteral("--:--.- / --:--.-"));

    const bool live = m_radioLive->isChecked();
    const std::string rawPathStd = rawPath.toStdString();

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
    m_btnStart->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_labelState->setText(QStringLiteral("SEARCHING"));
    AppendLog(live ? QStringLiteral("Started (live camera)") : QStringLiteral("Started (RAW playback)"));
}

void MainWindow::onStopClicked()
{
    if (!m_running)
    {
        return;
    }

    StopStream(QStringLiteral("Stopped"));
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

    if (!m_seekRangeKnown)
    {
        // RAW 파일을 연 직후에는 SDK가 탐색 범위를 아직 못 정했을 수 있으므로(Live 카메라라면
        // 계속 실패함), 준비될 때까지 매 폴링마다 다시 시도한다.
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

    m_btnStart->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_labelState->setText(QStringLiteral("IDLE"));
    m_seekRangeKnown = false;
    m_sliderPosition->setEnabled(false);
    m_sliderPosition->setValue(0);
    m_labelTime->setText(QStringLiteral("--:--.- / --:--.-"));
    AppendLog(logMessage);
}

void MainWindow::onBrowseRawClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select RAW file"),
        QString(),
        QStringLiteral("Metavision RAW (*.raw);;All Files (*)"));

    if (!path.isEmpty())
    {
        m_editRawPath->setText(path);
        m_radioRaw->setChecked(true);
    }
}

void MainWindow::onBrowseOutputClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select output folder"));

    if (!dir.isEmpty())
    {
        m_editOutputDir->setText(dir);
    }
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

    DrawFrame(msg->frame);

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

    if (su.justEnteredReady)
    {
        AppendLog(QStringLiteral("READY"));
    }

    if (su.justTriggered)
    {
        StartCaptureSave();
        AppendLog(QStringLiteral("TRIGGERED - capture started"));
    }

    if (su.state == ShotState::Capturing)
    {
        SaveCaptureFrame(msg->frame);
    }

    if (su.justFinishedCapture)
    {
        AppendLog(QStringLiteral("Capture finished: %1 frame(s) saved to %2")
            .arg(m_captureFrameIndex)
            .arg(m_currentCaptureDir));
        FinishCaptureSave();
    }
}
