#include "screen_recorder.h"

#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>

#include <QtOpenGL/QGLWidget>

#include <utils/common/warnings.h>

#include "ui/style/skin.h"
#include "video_recorder_settings.h"
#include "utils/common/log.h"
#include "device_plugins/desktop_win/desktop_file_encoder.h"

QnScreenRecorder::QnScreenRecorder(QObject *parent):
    QObject(parent),
    m_recording(false),
    m_encoder(NULL)
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
    delete m_encoder;
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

    if (!m_encoder->start()) {
        cl_log.log(m_encoder->lastErrorStr(), cl_logERROR);

        emit error(m_encoder->lastErrorStr());


        m_encoder->deleteLater();
        m_encoder = 0;
        return;
    }

    m_recording = true;
    emit recordingStarted();
#else
    Q_UNUSED(appWidget)
#endif
}

void QnScreenRecorder::stopRecording() {
    if(!isSupported()) {
        qnWarning("Screen recording is not supported on this platform.");
        return;
    }

    if(!m_recording)
        return; /* Stopping when nothing is being recorded is OK. */

    Q_ASSERT(m_encoder != NULL);

#ifdef Q_OS_WIN
    QString recordedFileName = m_encoder->fileName();
    m_encoder->stop();

    m_recording = false;
    emit recordingFinished(recordedFileName);
    m_encoder->deleteLater();
    m_encoder = 0;
#endif
}
