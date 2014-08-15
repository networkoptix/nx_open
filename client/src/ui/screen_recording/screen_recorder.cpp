#include "screen_recorder.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <QtOpenGL/QGLWidget>

#include <utils/common/warnings.h>

#include "ui/style/skin.h"
#include "video_recorder_settings.h"
#include "utils/common/log.h"
#include "plugins/resource/desktop_win/desktop_file_encoder.h"
#include "plugins/resource/desktop_win/desktop_resource.h"
#include "recording/stream_recorder.h"
#include "core/resource_management/resource_pool.h"

QnScreenRecorder::QnScreenRecorder(QObject *parent):
    QObject(parent),
    m_recording(false),
    m_dataProvider(0),
    m_recorder(0)
{}

QnScreenRecorder::~QnScreenRecorder() {
    stopRecording();
}

bool QnScreenRecorder::isSupported() {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool QnScreenRecorder::isRecording() const {
    return m_recording;
}

void QnScreenRecorder::startRecording() {
    if(m_recording) {
        qnWarning("Screen recording already in progress.");
        return;
    }

    if(!isSupported()) {
        qnWarning("Screen recording is not supported on this platform.");
        return;
    }

#ifdef Q_OS_WIN
    QnVideoRecorderSettings recorderSettings;

    QString filePath = recorderSettings.recordingFolder() + QLatin1String("/video_recording.avi");
    QnAudioDeviceInfo audioDevice = recorderSettings.primaryAudioDevice();
    QnAudioDeviceInfo secondAudioDevice;
    if (recorderSettings.secondaryAudioDevice().fullName() != audioDevice.fullName())
        secondAudioDevice = recorderSettings.secondaryAudioDevice();
    if (QnAudioDeviceInfo::availableDevices(QAudio::AudioInput).isEmpty()) {
        audioDevice = QnAudioDeviceInfo(); // no audio devices
        secondAudioDevice = QnAudioDeviceInfo();
    }

    QnDesktopResourcePtr res = qnResPool->getResourceById(QnDesktopResource::getDesktopResourceUuid()).dynamicCast<QnDesktopResource>();
    if (!res) {
        emit error(tr("Screen capturing subsystem is not initialized yet. Please try again later."));
        return;
    }

    m_dataProvider = dynamic_cast<QnDesktopDataProviderWrapper*> (res->createDataProvider(Qn::CR_Default));
    m_recorder = new QnStreamRecorder(res->toResourcePtr());
    m_dataProvider->addDataProcessor(m_recorder);
    m_recorder->setFileName(filePath);
    m_recorder->setContainer(lit("avi"));
    m_recorder->setRole(QnStreamRecorder::Role_FileExport);

    connect(m_recorder, &QnStreamRecorder::recordingFinished, this, &QnScreenRecorder::at_recorder_recordingFinished);

    m_dataProvider->start();

    if (!m_dataProvider->isInitialized()) {
        cl_log.log(m_dataProvider->lastErrorStr(), cl_logERROR);

        emit error(m_dataProvider->lastErrorStr());
        cleanupRecorder();
        return;
    }

    m_recorder->start();
    m_recording = true;
    emit recordingStarted();
#endif
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

void QnScreenRecorder::at_recorder_recordingFinished(int status, const QString &filename) {
    Q_UNUSED(filename)
    if (status == QnStreamRecorder::NoError)
        return;

    emit error(QnStreamRecorder::errorString(status));
    cleanupRecorder();
}

void QnScreenRecorder::stopRecording() {
    if(!isSupported()) {
        qnWarning("Screen recording is not supported on this platform.");
        return;
    }

    if(!m_recording)
        return; /* Stopping when nothing is being recorded is OK. */

    QString recordedFileName = m_recorder->getFileName();
    
    m_dataProvider->removeDataProcessor(m_recorder);

    delete m_dataProvider;
    delete m_recorder;

    m_dataProvider = 0;
    m_recorder = 0;

    m_recording = false;
    emit recordingFinished(recordedFileName);
}
