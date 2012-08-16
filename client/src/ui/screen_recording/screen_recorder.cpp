#include "screen_recorder.h"

#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>

#include <QtOpenGL/QGLWidget>

#include <utils/common/warnings.h>
#include "utils/client_util.h"

#include "ui/style/skin.h"
#include "video_recorder_settings.h"

#ifdef Q_OS_WIN
#   include <d3d9.h>
#   include <device_plugins/desktop/screen_grabber.h>
#   include <device_plugins/desktop/desktop_file_encoder.h>
#endif

namespace {
#ifdef Q_OS_WIN
    int screenToAdapter(int screen)
    {
        IDirect3D9* pD3D;
        if((pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
            return 0;

        QRect rect = qApp->desktop()->screenGeometry(screen);
        MONITORINFO monInfo;
        memset(&monInfo, 0, sizeof(monInfo));
        monInfo.cbSize = sizeof(monInfo);
        int rez = 0;

        for (int i = 0; i < qApp->desktop()->screenCount(); ++i) {
            if (!GetMonitorInfo(pD3D->GetAdapterMonitor(i), &monInfo))
                break;
            if (monInfo.rcMonitor.left == rect.left() && monInfo.rcMonitor.top == rect.top()) {
                rez = i;
                break;
            }
        }
        pD3D->Release();
        return rez;
    }
#endif

    QSize resolutionToSize(Qn::Resolution resolution) {
        QSize result(0, 0);
        switch(resolution) {
        case Qn::NativeResolution:          return QSize(0, 0);
        case Qn::QuaterNativeResolution:    return QSize(-2, -2);
        case Qn::Exact1920x1080Resolution:  return QSize(1920, 1080);
        case Qn::Exact1280x720Resolution:   return QSize(1280, 720);
        case Qn::Exact640x480Resolution:    return QSize(640, 480);
        case Qn::Exact320x240Resolution:    return QSize(320, 240);
        default:
            qnWarning("Invalid resolution value '%1', treating as native resolution.", static_cast<int>(resolution));
            return QSize(0, 0);
        }
    }

    float qualityToNumeric(Qn::DecoderQuality quality) {
        switch(quality) {
        case Qn::BestQuality:        return 1.0;
        case Qn::BalancedQuality:    return 0.75;
        case Qn::PerformanceQuality: return 0.5;
        default:
            qnWarning("Invalid quality value '%1', treating as best quality.", static_cast<int>(quality));
            return 1.0;
        }
    }

#ifdef Q_OS_WIN
    QnScreenGrabber::CaptureMode settingsToGrabberCaptureMode(Qn::CaptureMode captureMode) {
        switch(captureMode) {
        case Qn::WindowMode:             return QnScreenGrabber::CaptureMode_Application;
        case Qn::FullScreenMode:         return QnScreenGrabber::CaptureMode_DesktopWithAero;
        case Qn::FullScreenNoeroMode:    return QnScreenGrabber::CaptureMode_DesktopWithoutAero;
        default:
            qnWarning("Invalid capture mode value '%1', treating as window mode.", static_cast<int>(captureMode));
            return QnScreenGrabber::CaptureMode_Application;
        }
    }
#endif

} // anonymous namespace

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
    QString filePath = getTempRecordingDir() + QLatin1String("/video_recording.avi");

    QnVideoRecorderSettings recorderSettings;
    QAudioDeviceInfo audioDevice = recorderSettings.primaryAudioDevice();
    QAudioDeviceInfo secondAudioDevice = recorderSettings.secondaryAudioDevice();
    int screen = screenToAdapter(recorderSettings.screen());
    bool captureCursor = recorderSettings.captureCursor();
    QSize encodingSize = resolutionToSize(recorderSettings.resolution());
    float encodingQuality = qualityToNumeric(recorderSettings.decoderQuality());
    QnScreenGrabber::CaptureMode captureMode = settingsToGrabberCaptureMode(recorderSettings.captureMode());

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
