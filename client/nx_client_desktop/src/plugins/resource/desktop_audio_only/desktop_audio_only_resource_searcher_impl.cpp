#if !defined(Q_OS_WIN)

#include "desktop_audio_only_resource_searcher_impl.h"
#include <QGLWidget>
#include <QtMultimedia/QAudioDeviceInfo>

#include <nx/utils/unused.h>

#include <plugins/resource/desktop_audio_only/desktop_audio_only_resource.h>

QnDesktopResourceSearcherImpl::QnDesktopResourceSearcherImpl(QGLWidget* mainWindow)
{
    QN_UNUSED(mainWindow);
}

QnDesktopResourceSearcherImpl::~QnDesktopResourceSearcherImpl()
{
}

QnResourceList QnDesktopResourceSearcherImpl::findResources()
{
    QnResourceList result;
    auto availableDevices =
        QAudioDeviceInfo::availableDevices( QAudio::AudioInput );

    if (!availableDevices.isEmpty())
        result << QnResourcePtr(new QnDesktopAudioOnlyResource());

    return result;
}

#endif
