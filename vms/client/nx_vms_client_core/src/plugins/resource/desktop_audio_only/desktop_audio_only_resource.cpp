// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

AudioLayoutConstPtr QnDesktopAudioOnlyResource::getAudioLayout(
    const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return nullptr;
}

QnAbstractStreamDataProvider* QnDesktopAudioOnlyResource::createDataProvider(
    const QnResourcePtr& resource,
    Qn::ConnectionRole /*role*/)
{
    return new QnDesktopAudioOnlyDataProvider(resource);
}
