#include "workbench_screen_recording_handler.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <QtOpenGL/QGLWidget>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <ui/style/skin.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/screen_recording/video_recorder_settings.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_welcome_screen.h>
#include <ui/workbench/workbench_context.h>
#include <ui/widgets/main_window.h>

#include <nx/utils/string.h>
#include <nx/utils/log/log.h>
#include <utils/common/warnings.h>
#include <plugins/resource/desktop_win/desktop_resource.h>
#include <recording/stream_recorder.h>
#include <core/resource/file_processor.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/delayed.h>

using namespace nx::client::desktop::ui;

namespace {

/** Countdown value before screen recording starts. */
static const int kRecordingCountdownMs = 3000;

static const int kHideWhenMillisecondsLeft = 300;

static const int kTimerPrecisionMs = 100;

}

QnWorkbenchScreenRecordingHandler::QnWorkbenchScreenRecordingHandler(QObject *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),

    m_recording(false),
    m_timerId(0),
    m_dataProvider(nullptr),
    m_recorder(nullptr)
{
    const auto screenRecordingAction = action(action::ToggleScreenRecordingAction);
    if (!screenRecordingAction)
        return;

    display()->view()->window()->addAction(screenRecordingAction);

    connect(screenRecordingAction, &QAction::triggered, this,
        [this](bool checked)
        {
            if (checked)
                startRecordingCountdown();
            else
                stopRecording();
        });
}

QnWorkbenchScreenRecordingHandler::~QnWorkbenchScreenRecordingHandler()
{
    stopRecording();
}


void QnWorkbenchScreenRecordingHandler::timerEvent(QTimerEvent* event)
{
    base_type::timerEvent(event);

    NX_ASSERT(isRecordingCountdown());

    int millisecondsLeft = 0;
    if (m_countdown.isValid())
        millisecondsLeft = kRecordingCountdownMs - m_countdown.elapsed();

    const int seconds = std::max((millisecondsLeft + 500) / 1000, 0);
    const auto message = recordingCountdownText(seconds);

    if (const auto welcomeScreen = mainWindow()->welcomeScreen())
        welcomeScreen->setMessage(seconds > 0 ? message : QString());

    if (m_messageBox)
    {
        if (millisecondsLeft < kHideWhenMillisecondsLeft)
            m_messageBox->hideAnimated();

        m_messageBox->setText(recordingCountdownText(seconds));
    }

    if (seconds > 0)
        return;

    stopRecordingCountdown();

    // Give some time to render at least one frame  to make sure messagebox is not recorded.
    static const int kStartRecordingDelayMs = 50;
    executeDelayedParented([this]{startRecordingInternal();}, kStartRecordingDelayMs, this);
}

bool QnWorkbenchScreenRecordingHandler::isRecording() const
{
    return m_recording;
}

void QnWorkbenchScreenRecordingHandler::startRecordingCountdown()
{
    const auto screenRecordingAction = action(action::ToggleScreenRecordingAction);
    NX_ASSERT(screenRecordingAction);
    if (!screenRecordingAction)
        return;

    const auto desktop = resourcePool()->getResourceById<QnDesktopResource>(
        QnDesktopResource::getDesktopResourceUuid());
    if (!desktop)
    {
        QnGraphicsMessageBox::information(tr("Screen capturing subsystem is not initialized yet. "
            "Please try again later."), kRecordingCountdownMs);
        return;
    }

    if (isRecording() || isRecordingCountdown())
    {
        screenRecordingAction->setChecked(false);   // Stops recording
        return;
    }

    m_timerId = startTimer(kTimerPrecisionMs);
    const bool started = (m_timerId != 0);
    NX_ASSERT(started);
    if (!started)
    {
        stopRecordingCountdown();
        return;
    }

    int seconds = kRecordingCountdownMs / 1000;
    m_countdown.restart();
    m_messageBox = QnGraphicsMessageBox::information(
        recordingCountdownText(seconds), kRecordingCountdownMs);
}

void QnWorkbenchScreenRecordingHandler::stopRecordingCountdown()
{
    if (m_timerId != 0)
    {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    m_countdown.invalidate();

    if (m_messageBox)
        m_messageBox->hideImmideately();

    if (mainWindow() && mainWindow()->welcomeScreen())
        mainWindow()->welcomeScreen()->setMessage(QString());
}

bool QnWorkbenchScreenRecordingHandler::isRecordingCountdown() const
{
    return m_timerId != 0
        && m_countdown.isValid()
        && m_messageBox;
}

QString QnWorkbenchScreenRecordingHandler::recordingCountdownText(int seconds) const
{
    return tr("Recording in %1...").arg(seconds);
}

void QnWorkbenchScreenRecordingHandler::startRecordingInternal()
{
    if (m_recording)
    {
        qnWarning("Screen recording already in progress.");
        return;
    }

    const auto screenRecordingAction = action(action::ToggleScreenRecordingAction);
    if(!screenRecordingAction)
    {
        qnWarning("Screen recording is not supported on this platform.");
        return;
    }

    QnVideoRecorderSettings recorderSettings;

    QDateTime dt = QDateTime::currentDateTime();
    QString filePath = recorderSettings.recordingFolder() + QString(lit("/")) +
                       nx::utils::replaceNonFileNameCharacters(
                           QString(lit("video_recording_%1.avi"))
                           .arg(nx::utils::datetimeSaveDialogSuggestion(dt)), QLatin1Char('_'));
    QnAudioDeviceInfo audioDevice = recorderSettings.primaryAudioDevice();
    QnAudioDeviceInfo secondAudioDevice;
    if (recorderSettings.secondaryAudioDevice().fullName() != audioDevice.fullName())
        secondAudioDevice = recorderSettings.secondaryAudioDevice();
    if (QnAudioDeviceInfo::availableDevices(QAudio::AudioInput).isEmpty()) {
        audioDevice = QnAudioDeviceInfo(); // no audio devices
        secondAudioDevice = QnAudioDeviceInfo();
    }

    const QnDesktopResourcePtr res =
        resourcePool()->getResourceById<QnDesktopResource>(QnDesktopResource::getDesktopResourceUuid());

    if (!res)
    {
        onError(tr("Screen capturing subsystem is not initialized yet. Please try again later."));
        return;
    }

    m_dataProvider.reset(dynamic_cast<QnDesktopDataProviderWrapper*>(
        res->createDataProvider(Qn::CR_Default)));
    m_recorder.reset(new QnStreamRecorder(res->toResourcePtr()));
    m_dataProvider->addDataProcessor(m_recorder.data());
    m_recorder->addRecordingContext(filePath);
    m_recorder->setContainer(lit("avi"));
    m_recorder->setRole(StreamRecorderRole::fileExport);

    connect(m_recorder, &QnStreamRecorder::recordingFinished, this,
        &QnWorkbenchScreenRecordingHandler::onStreamRecordingFinished);

    m_dataProvider->start();

    if (!m_dataProvider->isInitialized()) {
        NX_ERROR(this, m_dataProvider->lastErrorStr());

        onError(m_dataProvider->lastErrorStr());
        return;
    }

    m_recorder->start();
    m_recording = true;
}

void QnWorkbenchScreenRecordingHandler::stopRecordingInternal()
{
    if (m_dataProvider && m_recorder)
        m_dataProvider->removeDataProcessor(m_recorder.data());

    m_recorder.reset();
    m_dataProvider.reset();
}

void QnWorkbenchScreenRecordingHandler::onStreamRecordingFinished(
    const StreamRecorderErrorStruct& status,
    const QString& filename)
{
    Q_UNUSED(filename)
    if (status.lastError == StreamRecorderError::noError)
        return;

    action(action::ToggleScreenRecordingAction)->setChecked(false);

    const auto errorReason = QnStreamRecorder::errorString(status.lastError);
    onError(errorReason);
}

void QnWorkbenchScreenRecordingHandler::stopRecording()
{
    stopRecordingCountdown();

    const auto screenRecordingAction = action(action::ToggleScreenRecordingAction);
    if (!screenRecordingAction)
    {
        qnWarning("Screen recording is not supported on this platform.");
        return;
    }

    const QString recordedFileName = (m_recorder
        ? m_recorder->fixedFileName()
        : QString());

    stopRecordingInternal();
    screenRecordingAction->setChecked(false);

    if(!m_recording)
        return; /* Stopping when nothing is being recorded is OK. */

    m_recording = false;

    onRecordingFinished(recordedFileName);
}

void QnWorkbenchScreenRecordingHandler::onRecordingFinished(const QString& fileName)
{
    QString suggetion = QFileInfo(fileName).fileName();
    if (suggetion.isEmpty())
        suggetion = tr("Recorded Video");

    while (true)
    {
        QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
            mainWindowWidget(),
            tr("Save Recording As..."),
            qnSettings->lastRecordingDir() + QLatin1Char('/') + suggetion,
            tr("AVI (Audio/Video Interleaved) (*.avi)")));

        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setAcceptMode(QFileDialog::AcceptSave);
        int dialogResult = dialog->exec();

        QString filePath = dialog->selectedFile();
        if (dialogResult == QDialog::Accepted && !filePath.isEmpty()) {
            QString selectedExtension = dialog->selectedExtension();
            if (!filePath.endsWith(selectedExtension, Qt::CaseInsensitive))
                filePath += selectedExtension;

            QFile::remove(filePath);
            if (!QFile::rename(fileName, filePath))
            {
                QnFileMessages::overwriteFailed(mainWindowWidget(), filePath);
                continue;
            }

            QnFileProcessor::createResourcesForFile(filePath);

            qnSettings->setLastRecordingDir(QFileInfo(filePath).absolutePath());
        }
        else {
            QFile::remove(fileName);
        }
        break;
    }
}

void QnWorkbenchScreenRecordingHandler::onError(const QString& reason)
{
    stopRecording();

    QnMessageBox::critical(mainWindowWidget(), tr("Failed to start recording"), reason);
}
