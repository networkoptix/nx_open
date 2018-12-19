#include "mobile_client_camera.h"

QnMobileClientCamera::QnMobileClientCamera(const QnUuid& resourceTypeId):
    base_type(resourceTypeId)
{
}

QnConstResourceVideoLayoutPtr QnMobileClientCamera::getVideoLayout(
    const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return QnMediaResource::getVideoLayout();
}

QnConstResourceAudioLayoutPtr QnMobileClientCamera::getAudioLayout(
    const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return QnMediaResource::getAudioLayout();
}

QnAbstractStreamDataProvider *QnMobileClientCamera::createLiveDataProvider()
{
    return nullptr;
}
