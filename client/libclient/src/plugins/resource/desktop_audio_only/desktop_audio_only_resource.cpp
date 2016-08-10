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

QnConstResourceAudioLayoutPtr QnDesktopAudioOnlyResource::getAudioLayout(const QnAbstractStreamDataProvider *dataProvider) const
{
    Q_UNUSED(dataProvider);
    QnConstResourceAudioLayoutPtr ptr(nullptr);
    return ptr;
}

QnAbstractStreamDataProvider* QnDesktopAudioOnlyResource::createDataProviderInternal(Qn::ConnectionRole role)
{
    Q_UNUSED(role);
    return new QnDesktopAudioOnlyDataProvider(toSharedPointer());
}
