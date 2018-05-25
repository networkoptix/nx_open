#include "video_recorder_settings.h"

#include <QtCore/QSettings>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#if defined(Q_OS_WIN)
#include <d3d9.h>
#endif // Q_OS_WIN

#include <utils/common/warnings.h>

QnVideoRecorderSettings::QnVideoRecorderSettings(QObject* parent):
    base_type(parent)
{
}

QnVideoRecorderSettings::~QnVideoRecorderSettings()
{
}

bool QnVideoRecorderSettings::captureCursor() const
{
    if (!settings.contains(QLatin1String("captureCursor")))
        return true;
    return settings.value(QLatin1String("captureCursor")).toBool();
}

void QnVideoRecorderSettings::setCaptureCursor(bool yes)
{
    settings.setValue(QLatin1String("captureCursor"), yes);
}

Qn::CaptureMode QnVideoRecorderSettings::captureMode() const
{
    const auto mode = static_cast<Qn::CaptureMode>(
        settings.value(QLatin1String("captureMode")).toInt());

    // Window mode support is removed
    return mode == Qn::WindowMode
        ? Qn::FullScreenMode
        : mode;
}

void QnVideoRecorderSettings::setCaptureMode(Qn::CaptureMode captureMode)
{
    settings.setValue(QLatin1String("captureMode"), captureMode);
}

Qn::DecoderQuality QnVideoRecorderSettings::decoderQuality() const
{
    if (!settings.contains(QLatin1String("decoderQuality")))
        return Qn::BalancedQuality;
    return (Qn::DecoderQuality) settings.value(QLatin1String("decoderQuality")).toInt();
}

void QnVideoRecorderSettings::setDecoderQuality(Qn::DecoderQuality decoderQuality)
{
    settings.setValue(QLatin1String("decoderQuality"), decoderQuality);
}

Qn::Resolution QnVideoRecorderSettings::resolution() const {
    Qn::Resolution rez = Qn::QuaterNativeResolution;
    if (settings.contains(lit("resolution")))
        rez = (Qn::Resolution) settings.value(lit("resolution")).toInt();
    return rez;
}

void QnVideoRecorderSettings::setResolution(Qn::Resolution resolution)
{
    settings.setValue(lit("resolution"), resolution);
}

int QnVideoRecorderSettings::screen() const
{
    int oldScreen = settings.value(QLatin1String("screen")).toInt();
    QRect geometry = settings.value(QLatin1String("screenResolution")).toRect();

    const auto screens = QGuiApplication::screens();
    if (screens.at(oldScreen)->geometry() == geometry)
        return oldScreen;

    for (int i = 0; i < screens.size(); i++)
    {
        if (screens.at(i)->geometry() == geometry)
            return i;
    }
    return screens.indexOf(QGuiApplication::primaryScreen());
}

void QnVideoRecorderSettings::setScreen(int screen)
{
    settings.setValue(QLatin1String("screen"), screen);
    settings.setValue(QLatin1String("screenResolution"),
        QGuiApplication::screens().at(screen)->geometry());
}

float QnVideoRecorderSettings::qualityToNumeric(Qn::DecoderQuality quality)
{
    switch(quality)
    {
        case Qn::BestQuality:
            return 1.0;
        case Qn::BalancedQuality:
            return 0.75;
        case Qn::PerformanceQuality:
            return 0.5;
        default:
            qnWarning("Invalid quality value '%1', treating as best quality.", static_cast<int>(quality));
            return 1.0;
    }
}

int QnVideoRecorderSettings::screenToAdapter(int screen)
{
#ifdef Q_OS_WIN
    IDirect3D9* pD3D;
    if((pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return 0;

    const auto screens = QGuiApplication::screens();
    QRect rect = screens.at(screen)->geometry();
    MONITORINFO monInfo;
    memset(&monInfo, 0, sizeof(monInfo));
    monInfo.cbSize = sizeof(monInfo);
    int rez = 0;

    for (int i = 0; i < screens.size(); ++i)
    {
        if (!GetMonitorInfo(pD3D->GetAdapterMonitor(i), &monInfo))
            break;
        if (monInfo.rcMonitor.left == rect.left() && monInfo.rcMonitor.top == rect.top()) {
            rez = i;
            break;
        }
    }
    pD3D->Release();
    return rez;
#else
    return screen;
#endif
}

QSize QnVideoRecorderSettings::resolutionToSize(Qn::Resolution resolution)
{
    QSize result(0, 0);
    switch(resolution)
    {
        case Qn::NativeResolution:
            return QSize(0, 0);
        case Qn::QuaterNativeResolution:
            return QSize(-2, -2);
        case Qn::Exact1920x1080Resolution:
            return QSize(1920, 1080);
        case Qn::Exact1280x720Resolution:
            return QSize(1280, 720);
        case Qn::Exact640x480Resolution:
            return QSize(640, 480);
        case Qn::Exact320x240Resolution:
            return QSize(320, 240);
        default:
            qnWarning("Invalid resolution value '%1', treating as native resolution.", static_cast<int>(resolution));
            return QSize(0, 0);
    }
}

