#include "mobile_client_camera.h"

QnMobileClientCamera::QnMobileClientCamera(const QnUuid &resourceTypeId)
    : base_type(resourceTypeId)
{}

QnConstResourceVideoLayoutPtr QnMobileClientCamera::getVideoLayout(const QnAbstractStreamDataProvider *dataProvider) const {
    Q_UNUSED(dataProvider)
    return QnMediaResource::getVideoLayout();
}

QnConstResourceAudioLayoutPtr QnMobileClientCamera::getAudioLayout(const QnAbstractStreamDataProvider *dataProvider) const {
    Q_UNUSED(dataProvider)
    return QnMediaResource::getAudioLayout();
}

QnAbstractStreamDataProvider *QnMobileClientCamera::createLiveDataProvider() {
    return nullptr;
}
