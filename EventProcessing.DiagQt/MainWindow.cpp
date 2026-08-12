#include "MainWindow.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

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
    if (bgrFrame.empty() || bgrFrame.type() != CV_8UC3)
    {
        return;
    }

    const cv::Mat safe = bgrFrame.isContinuous() ? bgrFrame : bgrFrame.clone();

    const QImage image(safe.data, safe.cols, safe.rows, static_cast<int>(safe.step), QImage::Format_BGR888);

    m_previewPixmap = QPixmap::fromImage(image);
    update();
}

void MainWindow::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    if (m_previewPixmap.isNull() || m_labelPreview == nullptr)
    {
        return;
    }

    const QRect rc = m_labelPreview->geometry();
    const QPixmap scaled = m_previewPixmap.scaled(rc.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPainter painter(this);
    painter.fillRect(rc, Qt::black);

    const int dx = rc.left() + (rc.width() - scaled.width()) / 2;
    const int dy = rc.top() + (rc.height() - scaled.height()) / 2;
    painter.drawPixmap(dx, dy, scaled);
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

    m_stream.Stop();
    m_running = false;

    m_btnStart->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_labelState->setText(QStringLiteral("IDLE"));
    AppendLog(QStringLiteral("Stopped"));
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

void MainWindow::OnFrameReady(std::shared_ptr<FrameMessage> msg)
{
    if (!msg || msg->frame.empty())
    {
        return;
    }

    DrawFrame(msg->frame);

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
