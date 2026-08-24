#include "EventProcessingDiagDlg.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QDoubleValidator>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QPushButton>
#include <QPixmap>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QShortcut>
#include <QVBoxLayout>

#include <algorithm>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using namespace eventcore;

namespace {
QString stateName(ShotState state)
{
    switch (state) {
    case ShotState::Searching: return QStringLiteral("SEARCHING");
    case ShotState::Ready: return QStringLiteral("READY");
    case ShotState::Capturing: return QStringLiteral("CAPTURING");
    }
    return QStringLiteral("?");
}

QLineEdit* numericEdit(const QString& value, QWidget* parent)
{
    auto* edit = new QLineEdit(value, parent);
    edit->setValidator(new QDoubleValidator(0.0, 1000000000.0, 6, edit));
    return edit;
}
}

EventProcessingDiagDialog::EventProcessingDiagDialog(QWidget* parent) : QDialog(parent)
{
    buildUi();
}

EventProcessingDiagDialog::~EventProcessingDiagDialog()
{
    stream_.Stop();
}

void EventProcessingDiagDialog::buildUi()
{
    setWindowTitle(tr("Event Camera Processing Diagnostic"));
    resize(1100, 760);

    liveRadio_ = new QRadioButton(tr("Live camera"), this);
    rawRadio_ = new QRadioButton(tr("RAW playback"), this);
    rawRadio_->setChecked(true);
    rawPath_ = new QLineEdit(this);
    auto* rawBrowse = new QPushButton(tr("Browse…"), this);
    auto* sourceRow = new QHBoxLayout;
    sourceRow->addWidget(liveRadio_);
    sourceRow->addWidget(rawRadio_);
    sourceRow->addWidget(rawPath_, 1);
    sourceRow->addWidget(rawBrowse);

    outputDirectory_ = new QLineEdit(QDir::current().filePath(QStringLiteral("output")), this);
    auto* outputBrowse = new QPushButton(tr("Browse…"), this);
    auto* outputRow = new QHBoxLayout;
    outputRow->addWidget(outputDirectory_, 1);
    outputRow->addWidget(outputBrowse);

    readySeconds_ = numericEdit(QStringLiteral("1.0"), this);
    captureSeconds_ = numericEdit(QStringLiteral("1.0"), this);
    stablePixels_ = numericEdit(QStringLiteral("15"), this);
    shotSpeed_ = numericEdit(QStringLiteral("1000"), this);
    missToleranceMs_ = numericEdit(QStringLiteral("150"), this);
    windowUs_ = numericEdit(QStringLiteral("10000"), this);
    auto* settings = new QGroupBox(tr("Trigger settings"), this);
    auto* form = new QFormLayout(settings);
    form->addRow(tr("Ready time (s)"), readySeconds_);
    form->addRow(tr("Capture time (s)"), captureSeconds_);
    form->addRow(tr("Stable movement (px)"), stablePixels_);
    form->addRow(tr("Shot speed (px/s)"), shotSpeed_);
    form->addRow(tr("Miss tolerance (ms)"), missToleranceMs_);
    form->addRow(tr("Event window (µs)"), windowUs_);

    state_ = new QLabel(QStringLiteral("IDLE"), this);
    state_->setAlignment(Qt::AlignCenter);
    state_->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: bold; padding: 8px;"));
    startButton_ = new QPushButton(tr("Start"), this);
    stopButton_ = new QPushButton(tr("Stop"), this);
    stopButton_->setEnabled(false);
    auto* controls = new QHBoxLayout;
    controls->addWidget(startButton_);
    controls->addWidget(stopButton_);

    preview_ = new QLabel(tr("No frame"), this);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(640, 360);
    preview_->setStyleSheet(QStringLiteral("background: black; color: #aaa;"));
    timeline_ = new QSlider(Qt::Horizontal, this);
    timeline_->setRange(0, 10000);
    timeline_->setEnabled(false);
    timelineLabel_ = new QLabel(QStringLiteral("--:--.--- / --:--.---"), this);
    timelineLabel_->setMinimumWidth(150);
    auto* timelineRow = new QHBoxLayout;
    timelineRow->addWidget(timeline_, 1);
    timelineRow->addWidget(timelineLabel_);
    log_ = new QListWidget(this);
    log_->setMinimumHeight(140);

    auto* side = new QVBoxLayout;
    side->addWidget(settings);
    side->addWidget(state_);
    side->addLayout(controls);
    side->addStretch();
    auto* content = new QHBoxLayout;
    content->addWidget(preview_, 1);
    content->addLayout(side);
    auto* root = new QVBoxLayout(this);
    root->addLayout(sourceRow);
    root->addLayout(outputRow);
    root->addLayout(content, 1);
    root->addLayout(timelineRow);
    root->addWidget(log_);

    connect(rawBrowse, &QPushButton::clicked, this, [this] { browseRaw(); });
    connect(outputBrowse, &QPushButton::clicked, this, [this] { browseOutput(); });
    connect(startButton_, &QPushButton::clicked, this, [this] { start(); });
    connect(stopButton_, &QPushButton::clicked, this, [this] { stop(); });
    connect(timeline_, &QSlider::sliderReleased, this, [this] { seekToSlider(); });
    auto* seekBackward = new QShortcut(QKeySequence(Qt::Key_Left), this);
    auto* seekForward = new QShortcut(QKeySequence(Qt::Key_Right), this);
    connect(seekBackward, &QShortcut::activated, this, [this] { seekRelative(-1000000); });
    connect(seekForward, &QShortcut::activated, this, [this] { seekRelative(1000000); });
}

ShotTriggerConfig EventProcessingDiagDialog::readConfig() const
{
    ShotTriggerConfig config;
    config.readySeconds = readySeconds_->text().toDouble();
    config.captureSeconds = captureSeconds_->text().toDouble();
    config.stableMovePx = stablePixels_->text().toFloat();
    config.shotSpeedPxPerSec = shotSpeed_->text().toFloat();
    config.missToleranceUs = static_cast<lli>(missToleranceMs_->text().toDouble() * 1000.0);
    return config;
}

void EventProcessingDiagDialog::appendLog(const QString& message)
{
    log_->addItem(QDateTime::currentDateTime().toString(QStringLiteral("[HH:mm:ss] ")) + message);
    log_->scrollToBottom();
}

void EventProcessingDiagDialog::updateState(ShotState state) { state_->setText(stateName(state)); }

void EventProcessingDiagDialog::displayFrame(const cv::Mat& bgrFrame)
{
    if (bgrFrame.empty() || bgrFrame.type() != CV_8UC3) return;
    cv::Mat rgb;
    cv::cvtColor(bgrFrame, rgb, cv::COLOR_BGR2RGB);
    const QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
    preview_->setPixmap(QPixmap::fromImage(image.copy()).scaled(preview_->size(), Qt::KeepAspectRatio,
                                                               Qt::SmoothTransformation));
}

void EventProcessingDiagDialog::start()
{
    if (running_) return;
    const QString raw = rawPath_->text().trimmed();
    if (rawRadio_->isChecked() && raw.isEmpty()) {
        appendLog(tr("Please choose a RAW file, or select Live camera."));
        return;
    }
    if (!QDir().mkpath(outputDirectory_->text())) {
        appendLog(tr("Could not create the output directory."));
        return;
    }
    trigger_ = ShotTrigger(readConfig());
    capturing_ = false;
    const QByteArray path = QFile::encodeName(raw);
    const lli window = std::max<lli>(1, windowUs_->text().toLongLong());
    const bool live = liveRadio_->isChecked();
    const bool ok = stream_.Start(live ? "" : path.constData(), window,
        [this](const EventProcessingResult& result, lli startUs, lli) {
            const cv::Mat frame = result.debugImage.clone();
            const BallDetectionResult ball = result.ball;
            QMetaObject::invokeMethod(this, [this, frame, ball, startUs] {
                handleFrame(frame, ball, startUs);
            }, Qt::QueuedConnection);
        });
    if (!ok) {
        appendLog(tr("Failed to start stream: %1").arg(QString::fromStdString(stream_.LastError())));
        return;
    }
    running_ = true;
    seekStartUs_ = seekEndUs_ = currentTimestampUs_ = 0;
    timeline_->setEnabled(false);
    timelineLabel_->setText(QStringLiteral("--:--.--- / --:--.---"));
    startButton_->setEnabled(false);
    stopButton_->setEnabled(true);
    state_->setText(QStringLiteral("SEARCHING"));
    appendLog(live ? tr("Started (live camera)") : tr("Started (RAW playback)"));
}

void EventProcessingDiagDialog::stop()
{
    if (!running_) return;
    stream_.Stop();
    running_ = false;
    capturing_ = false;
    startButton_->setEnabled(true);
    stopButton_->setEnabled(false);
    state_->setText(QStringLiteral("IDLE"));
    timeline_->setEnabled(false);
    appendLog(tr("Stopped"));
}

void EventProcessingDiagDialog::browseRaw()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Select RAW recording"), {},
                                                      tr("Metavision RAW (*.raw);;All files (*)"));
    if (!path.isEmpty()) { rawPath_->setText(path); rawRadio_->setChecked(true); }
}

void EventProcessingDiagDialog::browseOutput()
{
    const QString path = QFileDialog::getExistingDirectory(this, tr("Select output folder"),
                                                            outputDirectory_->text());
    if (!path.isEmpty()) outputDirectory_->setText(path);
}

void EventProcessingDiagDialog::startCaptureSave()
{
    captureDirectory_ = QDir(outputDirectory_->text()).filePath(
        QDateTime::currentDateTime().toString(QStringLiteral("'shot_'yyyyMMdd_HHmmss")));
    capturing_ = QDir().mkpath(captureDirectory_);
    captureFrameIndex_ = 0;
    if (!capturing_) appendLog(tr("Could not create capture directory: %1").arg(captureDirectory_));
}

void EventProcessingDiagDialog::saveCaptureFrame(const cv::Mat& frame)
{
    if (!capturing_ || frame.empty()) return;
    const QString name = QDir(captureDirectory_).filePath(
        QStringLiteral("frame_%1.png").arg(captureFrameIndex_, 4, 10, QLatin1Char('0')));
    if (!cv::imwrite(QFile::encodeName(name).constData(), frame)) appendLog(tr("Failed to save %1").arg(name));
    ++captureFrameIndex_;
}

void EventProcessingDiagDialog::handleFrame(const cv::Mat& frame, const BallDetectionResult& ball, lli startUs)
{
    if (!running_) return;
    displayFrame(frame);
    updateTimeline(startUs);
    const ShotUpdateResult update = trigger_.Update(ball, startUs);
    updateState(update.state);
    if (update.justEnteredReady) appendLog(QStringLiteral("READY"));
    if (update.justTriggered) { startCaptureSave(); appendLog(tr("TRIGGERED - capture started")); }
    if (update.state == ShotState::Capturing) saveCaptureFrame(frame);
    if (update.justFinishedCapture) {
        appendLog(tr("Capture finished: %1 frame(s) saved to %2").arg(captureFrameIndex_).arg(captureDirectory_));
        capturing_ = false;
    }
}

void EventProcessingDiagDialog::updateTimeline(lli timestampUs)
{
    currentTimestampUs_ = timestampUs;
    if (rawRadio_->isChecked() && seekEndUs_ <= seekStartUs_) {
        if (stream_.GetSeekRange(seekStartUs_, seekEndUs_)) timeline_->setEnabled(true);
    }
    if (seekEndUs_ <= seekStartUs_) return;

    const double ratio = std::clamp(static_cast<double>(timestampUs - seekStartUs_) /
                                    static_cast<double>(seekEndUs_ - seekStartUs_), 0.0, 1.0);
    if (!timeline_->isSliderDown()) {
        const QSignalBlocker blocker(timeline_);
        timeline_->setValue(static_cast<int>(ratio * timeline_->maximum()));
    }
    const auto formatTime = [](lli us) {
        const qint64 ms = std::max<lli>(0, us) / 1000;
        return QStringLiteral("%1:%2.%3").arg(ms / 60000, 2, 10, QLatin1Char('0'))
            .arg((ms / 1000) % 60, 2, 10, QLatin1Char('0')).arg(ms % 1000, 3, 10, QLatin1Char('0'));
    };
    timelineLabel_->setText(formatTime(timestampUs - seekStartUs_) + QStringLiteral(" / ") +
                            formatTime(seekEndUs_ - seekStartUs_));
}

void EventProcessingDiagDialog::seekToSlider()
{
    if (!timeline_->isEnabled() || seekEndUs_ <= seekStartUs_) return;
    const lli target = seekStartUs_ + static_cast<lli>(
        static_cast<double>(timeline_->value()) / timeline_->maximum() * (seekEndUs_ - seekStartUs_));
    if (!stream_.Seek(target)) appendLog(tr("RAW seek failed; the recording index may not be ready."));
    else { trigger_.Reset(); capturing_ = false; updateState(ShotState::Searching); updateTimeline(target); }
}

void EventProcessingDiagDialog::seekRelative(lli deltaUs)
{
    if (!running_ || !rawRadio_->isChecked() || !timeline_->isEnabled()) return;
    const lli target = std::clamp(currentTimestampUs_ + deltaUs, seekStartUs_, seekEndUs_);
    if (stream_.Seek(target)) {
        trigger_.Reset();
        capturing_ = false;
        updateState(ShotState::Searching);
        updateTimeline(target);
    }
}

void EventProcessingDiagDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Left) { seekRelative(-1000000); event->accept(); return; }
    if (event->key() == Qt::Key_Right) { seekRelative(1000000); event->accept(); return; }
    QDialog::keyPressEvent(event);
}

void EventProcessingDiagDialog::closeEvent(QCloseEvent* event)
{
    stop();
    event->accept();
}
