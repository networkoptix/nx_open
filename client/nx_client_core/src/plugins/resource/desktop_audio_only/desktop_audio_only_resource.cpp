#include "desktop_audio_only_resource.h"
#include <plugins/resource/desktop_audio_only/desktop_audio_only_data_provider.h>

QnDesktopAudioOnlyResource::QnDesktopAudioOnlyResource()
{

}

QnDesktopAudioOnlyResource::~QnDesktopAudioOnlyResource()
{

}

bool QnDesktopAudioOnlyResource::isRendererSlow() const
{
    return false;
}

bool QnDesktopAudioOnlyResource::hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return false;
}

QnConstResourceAudioLayoutPtr QnDesktopAudioOnlyResource::getAudioLayout(const QnAbstractStreamDataProvider *dataProvider) const
{
    Q_UNUSED(dataProvider);
    QnConstResourceAudioLayoutPtr ptr(nullptr);
    return ptr;
}

QnAbstractStreamDataProvider* QnDesktopAudioOnlyResource::createDataProvider(
    const QnResourcePtr& resource,
    Qn::ConnectionRole /*role*/)
{
    return new QnDesktopAudioOnlyDataProvider(resource);
}
