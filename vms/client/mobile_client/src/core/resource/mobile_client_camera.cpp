// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_client_camera.h"

QnMobileClientCamera::QnMobileClientCamera(const nx::Uuid& resourceTypeId):
    base_type(resourceTypeId)
{
}

QnConstResourceVideoLayoutPtr QnMobileClientCamera::getVideoLayout(
    const QnAbstractStreamDataProvider* /*dataProvider*/)
{
    return QnMediaResource::getVideoLayout();
}

AudioLayoutConstPtr QnMobileClientCamera::getAudioLayout(
    const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return QnMediaResource::getAudioLayout();
}

QnAbstractStreamDataProvider *QnMobileClientCamera::createLiveDataProvider()
{
    return nullptr;
}
