// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_screen_recording_handler.h"

#include <chrono>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>

#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <client_core/client_core_module.h>
#include <core/dataprovider/data_provider_factory.h>
#include <core/resource/file_processor.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_data_provider_base.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/desktop/export/tools/export_storage_stream_recorder.h>
#include <nx/vms/client/desktop/resource/screen_recording/desktop_data_provider_wrapper.h>
#include <nx/vms/client/desktop/resource/screen_recording/video_recorder_settings.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <nx/vms/time/formatter.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <utils/common/delayed.h>

using namespace std::chrono;
using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

namespace {

/** Countdown value before screen recording starts. */
static constexpr auto kRecordingCountdown = 3s;

static const int kHideWhenMillisecondsLeft = 300;

static const int kTimerPrecisionMs = 100;

// TODO: #sivanov Code duplication between this module and action manager.
bool screenRecordingSupported()
{
    return nx::build_info::isWindows() && qnRuntime->isDesktopMode();
}

} // namespace

QnWorkbenchScreenRecordingHandler::QnWorkbenchScreenRecordingHandler(QObject *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),

    m_recording(false),
    m_timerId(0),
    m_dataProvider(nullptr),
    m_recorder(nullptr)
{
    if (!screenRecordingSupported())
        return;

    const auto screenRecordingAction = action(ui::action::ToggleScreenRecordingAction);
    if (!NX_ASSERT(screenRecordingAction))
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
    if (screenRecordingSupported())
        stopRecording();
}

void QnWorkbenchScreenRecordingHandler::timerEvent(QTimerEvent* event)
{
    base_type::timerEvent(event);

    NX_ASSERT(isRecordingCountdown());

    int millisecondsLeft = 0;
    if (m_countdown.isValid())
        millisecondsLeft = milliseconds(kRecordingCountdown).count() - m_countdown.elapsed();

    const int seconds = std::max((millisecondsLeft + 500) / 1000, 0);
    const auto message = recordingCountdownText(seconds);

    if (const auto welcomeScreen = mainWindow()->welcomeScreen())
        welcomeScreen->setMessage(seconds > 0 ? message : QString());

    if (m_messageBox)
    {
        if (millisecondsLeft < kHideWhenMillisecondsLeft)
            m_messageBox->hide();

        m_messageBox->changeText(recordingCountdownText(seconds));
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
    const auto screenRecordingAction = action(ui::action::ToggleScreenRecordingAction);
    NX_ASSERT(screenRecordingAction);
    if (!screenRecordingAction)
        return;

    const auto desktop = resourcePool()->getResourceById<core::DesktopResource>(
        core::DesktopResource::getDesktopResourceUuid());

    if (!desktop)
    {
        SceneBanner::show(tr("Screen capturing subsystem is not initialized yet. "
            "Please try again later."), kRecordingCountdown);
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

    const int seconds = std::chrono::seconds(kRecordingCountdown).count();
    m_countdown.restart();
    m_messageBox = SceneBanner::show(recordingCountdownText(seconds), kRecordingCountdown);
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
        m_messageBox->hide(/*immediately*/ true);

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
        NX_ASSERT(false, "Screen recording already in progress.");
        return;
    }

    const auto screenRecordingAction = action(ui::action::ToggleScreenRecordingAction);
    if (!NX_ASSERT(screenRecordingAction))
        return;

    VideoRecorderSettings recorderSettings;

    QDateTime dt = QDateTime::currentDateTime();
    QString filePath = recorderSettings.recordingFolder() + QString(lit("/")) +
        nx::utils::replaceNonFileNameCharacters(
            QString(lit("video_recording_%1.avi"))
                .arg(nx::vms::time::toString(dt, nx::vms::time::Format::filename_date)), QLatin1Char('_'));
    core::AudioDeviceInfo audioDevice = recorderSettings.primaryAudioDevice();
    core::AudioDeviceInfo secondAudioDevice;
    if (recorderSettings.secondaryAudioDevice().fullName() != audioDevice.fullName())
        secondAudioDevice = recorderSettings.secondaryAudioDevice();
    if (core::AudioDeviceInfo::availableDevices(QAudio::AudioInput).isEmpty()) {
        audioDevice = core::AudioDeviceInfo(); // no audio devices
        secondAudioDevice = core::AudioDeviceInfo();
    }

    const auto res = resourcePool()->getResourceById<core::DesktopResource>(
        core::DesktopResource::getDesktopResourceUuid());

    if (!res)
    {
        onError(tr("Screen capturing subsystem is not initialized yet. Please try again later."));
        return;
    }

    m_dataProvider.reset(dynamic_cast<DesktopDataProviderWrapper*>(
        qnClientCoreModule->dataProviderFactory()->createDataProvider(res)));
    m_recorder.reset(new nx::vms::client::desktop::ExportStorageStreamRecorder(res->toResourcePtr(), m_dataProvider.get()));
    m_dataProvider->addDataProcessor(m_recorder.data());

    if (!m_recorder->addRecordingContext(filePath))
    {
        onError(tr("Could not open file '%1'. Please check if the folder is accessible").arg(filePath));
        return;
    }

    m_recorder->setContainer(lit("avi"));

    connect(m_recorder.get(), &QnStreamRecorder::recordingStarted, this,
        &QnWorkbenchScreenRecordingHandler::onStreamRecordingStarted);
    connect(m_recorder.get(), &QnStreamRecorder::recordingFinished, this,
        &QnWorkbenchScreenRecordingHandler::onStreamRecordingFinished);

    connect(m_dataProvider->owner(), &QnAbstractStreamDataProvider::finished, this,
        &QnWorkbenchScreenRecordingHandler::onProviderFinished);
    m_dataProvider->start();

    if (!m_dataProvider->isInitialized()) {
        NX_ERROR(this, m_dataProvider->lastErrorStr());

        onError(m_dataProvider->lastErrorStr());
        return;
    }

    m_recorder->start();
}

void QnWorkbenchScreenRecordingHandler::stopRecordingInternal()
{
    if (m_dataProvider && m_recorder)
    {
        m_recorder->stop();
        m_dataProvider->removeDataProcessor(m_recorder.data());
    }

    m_recorder.reset();
    m_dataProvider.reset();
}

void QnWorkbenchScreenRecordingHandler::onStreamRecordingStarted()
{
    m_recording = true;
}

void QnWorkbenchScreenRecordingHandler::onStreamRecordingFinished(
    const std::optional<nx::recording::Error>& status,
    const QString& filename)
{
    if (!status)
    {
        m_recordedFileName = filename;
        return;
    }

    const auto screenRecordingAction = action(ui::action::ToggleScreenRecordingAction);
    if (!NX_ASSERT(screenRecordingAction))
        return;

    screenRecordingAction->setChecked(false);

    onError(status->toString());
}

void QnWorkbenchScreenRecordingHandler::onProviderFinished()
{
    if (m_dataProvider && !m_dataProvider->lastErrorStr().isEmpty())
    {
        NX_ERROR(this, m_dataProvider->lastErrorStr());
        onError(m_dataProvider->lastErrorStr());
    }
}

void QnWorkbenchScreenRecordingHandler::stopRecording()
{
    stopRecordingCountdown();

    stopRecordingInternal();
    if (const auto screenRecordingAction = action(ui::action::ToggleScreenRecordingAction))
        screenRecordingAction->setChecked(false);

    if (!m_recording)
        return; /* Stopping when nothing is being recorded is OK. */

    m_recording = false;

    onRecordingFinished();
}

void QnWorkbenchScreenRecordingHandler::onRecordingFinished()
{
    QString suggestion = QFileInfo(m_recordedFileName).fileName();
    if (suggestion.isEmpty())
        suggestion = tr("Recorded Video");

    QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
        mainWindowWidget(),
        tr("Save Recording As..."),
        qnSettings->lastRecordingDir() + QLatin1Char('/') + suggestion,
        QnCustomFileDialog::createFilter(tr("AVI (Audio/Video Interleaved)"), "avi")));

    dialog->setOption(QFileDialog::DontConfirmOverwrite);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);

    while (true)
    {
        if (dialog->exec() == QDialog::Accepted)
        {
            QString filePath = dialog->selectedFile();

            const QString selectedExtension = dialog->selectedExtension();
            if (!filePath.endsWith(selectedExtension, Qt::CaseInsensitive))
                filePath += selectedExtension;

            if (QFile::exists(filePath))
            {
                if (!QnFileMessages::confirmOverwrite(mainWindowWidget(), filePath))
                    continue;

                QFile::remove(filePath);
            }

            if (!QFile::rename(m_recordedFileName, filePath))
            {
                QnFileMessages::overwriteFailed(mainWindowWidget(), filePath);
                continue;
            }

            QnFileProcessor::createResourcesForFile(filePath, resourcePool());

            qnSettings->setLastRecordingDir(QFileInfo(filePath).absolutePath());

            break;
        }
        else
        {
            QFile::remove(m_recordedFileName);
            break;
        }
    }

    m_recordedFileName.clear();
}

void QnWorkbenchScreenRecordingHandler::onError(const QString& reason)
{
    m_recording = false; //< Don't open save dialog.
    stopRecording();
    QnMessageBox::critical(mainWindowWidget(), tr("Failed to start recording"), reason);
}
