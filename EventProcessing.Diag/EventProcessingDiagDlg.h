#pragma once

#include "LiveEventStream.h"
#include "ShotTrigger.h"

#include <QDialog>
#include <QImage>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QCloseEvent;

class EventProcessingDiagDialog final : public QDialog
{
public:
    explicit EventProcessingDiagDialog(QWidget* parent = nullptr);
    ~EventProcessingDiagDialog() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    eventcore::ShotTriggerConfig readConfig() const;
    void appendLog(const QString& message);
    void updateState(eventcore::ShotState state);
    void displayFrame(const cv::Mat& bgrFrame);
    void start();
    void stop();
    void browseRaw();
    void browseOutput();
    void handleFrame(const cv::Mat& frame, const eventcore::BallDetectionResult& ball,
                     eventcore::lli windowStartUs);
    void startCaptureSave();
    void saveCaptureFrame(const cv::Mat& frame);

    eventcore::LiveEventStream stream_;
    eventcore::ShotTrigger trigger_;
    bool running_ = false;
    bool capturing_ = false;
    int captureFrameIndex_ = 0;
    QString captureDirectory_;

    QRadioButton* liveRadio_ = nullptr;
    QRadioButton* rawRadio_ = nullptr;
    QLineEdit* rawPath_ = nullptr;
    QLineEdit* outputDirectory_ = nullptr;
    QLineEdit* readySeconds_ = nullptr;
    QLineEdit* captureSeconds_ = nullptr;
    QLineEdit* stablePixels_ = nullptr;
    QLineEdit* shotSpeed_ = nullptr;
    QLineEdit* missToleranceMs_ = nullptr;
    QLineEdit* windowUs_ = nullptr;
    QLabel* preview_ = nullptr;
    QLabel* state_ = nullptr;
    QListWidget* log_ = nullptr;
    QPushButton* startButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
};
