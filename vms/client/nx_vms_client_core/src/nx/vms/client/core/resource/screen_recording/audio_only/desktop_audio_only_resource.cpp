// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_audio_only_resource.h"

#include "desktop_audio_only_data_provider.h"

namespace nx::vms::client::core {

DesktopAudioOnlyResource::DesktopAudioOnlyResource()
{

}

DesktopAudioOnlyResource::~DesktopAudioOnlyResource()
{

}

bool DesktopAudioOnlyResource::isRendererSlow() const
{
    return false;
}

bool DesktopAudioOnlyResource::hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return false;
}

AudioLayoutConstPtr DesktopAudioOnlyResource::getAudioLayout(
    const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return nullptr;
}

QnAbstractStreamDataProvider* DesktopAudioOnlyResource::createDataProvider(
    const QnResourcePtr& resource,
    Qn::ConnectionRole /*role*/)
{
    return new DesktopAudioOnlyDataProvider(resource);
}

} // namespace nx::vms::client::core
