#include "screen_recording_handler.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <QtOpenGL/QGLWidget>

#include <client/client_settings.h>
#include <ui/style/skin.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/screen_recording/video_recorder_settings.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/workbench/workbench_display.h>
#include <nx/utils/string.h>
#include <nx/utils/log/log.h>
#include <utils/common/warnings.h>
#include <plugins/resource/desktop_win/desktop_resource.h>
#include <recording/stream_recorder.h>
#include <core/resource/file_processor.h>
#include <core/resource_management/resource_pool.h>

namespace {
/** Countdown value before screen recording starts. */
static const int kRecordingCountdownMs = 3000;

}

QnScreenRecorder::QnScreenRecorder(QObject *parent) :
    QObject(parent),
    base_type(parent),

    m_recording(false),
    m_dataProvider(0),
    m_recorder(0),
    m_recordingCountdownLabel(nullptr)
{
    const auto screenRecordingAction = action(QnActions::ToggleScreenRecordingAction);
    if (!screenRecordingAction)
        return;

    connect(this, &QnScreenRecorder::recordingFinished, this, &QnScreenRecorder::onRecordingFinished);

    const auto view = display()->view();
    view->window()->addAction(screenRecordingAction);

    connect(this, &QnScreenRecorder::error, this,
        [this, view](const QString& errorReason)
        {
            QnMessageBox::warning(display()->view(), tr("Warning"),
                tr("Unable to start recording due to the following error: %1").arg(errorReason));
        });

    connect(screenRecordingAction, &QAction::triggered, this,
        [this](bool checked)
        {
            if (checked)
                startRecording();
            else
                stopRecording();
        });
}

QnScreenRecorder::~QnScreenRecorder()
{
    stopRecording();
}


bool QnScreenRecorder::isRecording() const
{
    return m_recording;
}

void QnScreenRecorder::startRecording()
{
    const auto screenRecordingAction = action(QnActions::ToggleScreenRecordingAction);
    if (!screenRecordingAction)
        return;

    const auto desktop = qnResPool->getResourceById<QnDesktopResource>(
        QnDesktopResource::getDesktopResourceUuid());
    if (!desktop)
    {
        QnGraphicsMessageBox::information(tr("Screen capturing subsystem is not initialized yet. "
            "Please try again later."), kRecordingCountdownMs);
        return;
    }

    if (isRecording() || m_recordingCountdownLabel)
    {
        screenRecordingAction->setChecked(false);
        return;
    }

    screenRecordingAction->setChecked(true); //? do i need this line? - there is no start screen recording call in other place
    m_recordingCountdownLabel = QnGraphicsMessageBox::informationTicking(
        tr("Recording in...%1"), kRecordingCountdownMs);
    connect(m_recordingCountdownLabel, &QnGraphicsMessageBox::finished,
        this, &QnScreenRecorder::onRecordingAnimationFinished);
}

void QnScreenRecorder::startRecordingInternal()
{
    if(m_recording)
    {
        qnWarning("Screen recording already in progress.");
        return;
    }

    const auto screenRecordingAction = action(QnActions::ToggleScreenRecordingAction);
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

    QnDesktopResourcePtr res = qnResPool->getResourceById<QnDesktopResource>(QnDesktopResource::getDesktopResourceUuid());
    if (!res) {
        emit error(tr("Screen capturing subsystem is not initialized yet. Please try again later."));
        return;
    }

    m_dataProvider = dynamic_cast<QnDesktopDataProviderWrapper*> (res->createDataProvider(Qn::CR_Default));
    m_recorder = new QnStreamRecorder(res->toResourcePtr());
    m_dataProvider->addDataProcessor(m_recorder);
    m_recorder->addRecordingContext(filePath);
    m_recorder->setContainer(lit("avi"));
    m_recorder->setRole(QnStreamRecorder::Role_FileExport);

    connect(m_recorder, &QnStreamRecorder::recordingFinished, this,
        &QnScreenRecorder::onStreamRecordingFinished);

    m_dataProvider->start();

    if (!m_dataProvider->isInitialized()) {
        cl_log.log(m_dataProvider->lastErrorStr(), cl_logERROR);

        emit error(m_dataProvider->lastErrorStr());
        cleanupRecorder();
        return;
    }

    m_recorder->start();
    m_recording = true;

    if (!m_recordingCountdownLabel)
        m_recordingCountdownLabel->setOpacity(0.0);

    emit recordingStarted();
}

void QnScreenRecorder::cleanupRecorder()
{
    if (m_dataProvider && m_recorder)
        m_dataProvider->removeDataProcessor(m_recorder);
    delete m_recorder;
    delete m_dataProvider;
    m_recorder = 0;
    m_dataProvider = 0;
}

void QnScreenRecorder::onStreamRecordingFinished(
    const QnStreamRecorder::ErrorStruct& status,
    const QString& filename)
{
    Q_UNUSED(filename)
    if (status.lastError == QnStreamRecorder::NoError)
        return;

    action(QnActions::ToggleScreenRecordingAction)->setChecked(false);

    const auto errorReason = QnStreamRecorder::errorString(status.lastError);
    emit error(errorReason);
    cleanupRecorder();
}

void QnScreenRecorder::stopRecording()
{
    if (m_recordingCountdownLabel)
    {
        disconnect(m_recordingCountdownLabel, nullptr, this, nullptr);
        m_recordingCountdownLabel->hideImmideately();
        m_recordingCountdownLabel = nullptr;
    }

    const auto screenRecordingAction = action(QnActions::ToggleScreenRecordingAction);
    if (!screenRecordingAction)
    {
        qnWarning("Screen recording is not supported on this platform.");
        return;
    }

    screenRecordingAction->setChecked(false);
    if(!m_recording)
        return; /* Stopping when nothing is being recorded is OK. */

    QString recordedFileName = m_recorder->fixedFileName();

    m_dataProvider->removeDataProcessor(m_recorder);

    delete m_dataProvider;
    delete m_recorder;

    m_dataProvider = 0;
    m_recorder = 0;

    m_recording = false;

    emit recordingFinished(recordedFileName);
}

void QnScreenRecorder::onRecordingFinished(const QString& fileName)
{
    QString suggetion = QFileInfo(fileName).fileName();
    if (suggetion.isEmpty())
        suggetion = tr("Recorded Video");

    while (true)
    {
        QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
            display()->view(),
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
            if (!QFile::rename(fileName, filePath)) {
                QString message = tr("Could not overwrite file '%1'. Please try a different name.").arg(filePath);
                CL_LOG(cl_logWARNING) cl_log.log(message, cl_logWARNING);
                QnMessageBox::warning(display()->view(), tr("Warning"), message, QDialogButtonBox::Ok, QDialogButtonBox::NoButton);
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

void QnScreenRecorder::onRecordingAnimationFinished()
{
    if (m_recordingCountdownLabel)
    {
        m_recordingCountdownLabel->setOpacity(0.0);
        m_recordingCountdownLabel = nullptr;
    }

    startRecordingInternal();
}

