#include "screen_recorder.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <QtOpenGL/QGLWidget>

#include <utils/common/warnings.h>

#include "ui/style/skin.h"
#include "video_recorder_settings.h"
#include "utils/common/log.h"
#include "device_plugins/desktop_windows_specific/desktop_file_encoder.h"
#include "device_plugins/desktop_windows_specific/device/desktop_resource.h"
#include "recording/stream_recorder.h"
#include "core/resource_managment/resource_pool.h"

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

void QnScreenRecorder::startRecording(QGLWidget *appWidget) {
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
    int screen = QnVideoRecorderSettings::screenToAdapter(recorderSettings.screen());
    bool captureCursor = recorderSettings.captureCursor();
    QSize encodingSize = QnVideoRecorderSettings::resolutionToSize(recorderSettings.resolution());
    float encodingQuality = QnVideoRecorderSettings::qualityToNumeric(recorderSettings.decoderQuality());
    Qn::CaptureMode captureMode = recorderSettings.captureMode();

    QPixmap logo;
#if defined(CL_TRIAL_MODE) || defined(CL_FORCE_LOGO)
    //QString logoName = QString("logo_") + QString::number(encodingSize.width()) + QString("_") + QString::number(encodingSize.height()) + QString(".png");
    QString logoName = QLatin1String("logo_1920_1080.png");
    logo = qnSkin->pixmap(logoName); // hint: comment this line to remove logo
#endif
    //delete m_encoder;

    /*
    m_encoder = new QnDesktopFileEncoder(
        filePath,
        screen,
        audioDevice.isNull() ? 0 : &audioDevice,
        secondAudioDevice.isNull() ? 0 : &secondAudioDevice,
        captureMode,
        captureCursor,
        encodingSize,
        encodingQuality,
        appWidget,
        logo
    );
    */

    QnDesktopResourcePtr res = qnResPool->getResourceByGuid(QnDesktopResource().getGuid()).dynamicCast<QnDesktopResource>();
    if (!res) {
        emit error(tr("Screen capturing subsystem is not initialized yet. Please try latter"));
        return;
    }

    m_dataProvider = dynamic_cast<QnDesktopDataProviderWrapper*> (res->createDataProvider(QnResource::Role_Default));
    m_recorder = new QnStreamRecorder(res->toResourcePtr());
    m_dataProvider->addDataProcessor(m_recorder);
    m_recorder->setFileName(filePath);
    m_recorder->setContainer(lit("avi"));
    m_recorder->setRole(QnStreamRecorder::Role_FileExport);

    connect(m_recorder, SIGNAL(recordingFailed(QString)), this, SLOT(onRecordingFailed(QString)));
    connect(m_recorder, SIGNAL(recordingFinished(QString)), this, SLOT(onRecordingFinished(QString)));

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
#else
    Q_UNUSED(appWidget)
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

void QnScreenRecorder::onRecordingFailed(QString msg)
{
    emit error(msg);
    cleanupRecorder();
}

void QnScreenRecorder::onRecordingFinished(QString)
{
}

void QnScreenRecorder::stopRecording() {
    if(!isSupported()) {
        qnWarning("Screen recording is not supported on this platform.");
        return;
    }

    if(!m_recording)
        return; /* Stopping when nothing is being recorded is OK. */

#ifdef Q_OS_WIN
    QString recordedFileName = m_recorder->getFileName();
    
    m_dataProvider->removeDataProcessor(m_recorder);

    delete m_dataProvider;
    delete m_recorder;

    m_dataProvider = 0;
    m_recorder = 0;

    m_recording = false;
    emit recordingFinished(recordedFileName);
#endif
}
