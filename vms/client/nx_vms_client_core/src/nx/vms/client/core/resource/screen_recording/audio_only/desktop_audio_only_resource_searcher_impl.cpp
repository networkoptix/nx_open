// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_audio_only_resource_searcher_impl.h"

#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QMediaDevices>

#include <nx/build_info.h>

#include "desktop_audio_only_resource.h"

namespace nx::vms::client::core {

namespace {

bool hasAudioInputs()
{
    // Assume that we have a mic on mobile platforms to avoid requesting permissions at app launch.
    if (nx::build_info::isAndroid() || nx::build_info::isIos())
        return true;

    return !QMediaDevices::audioInputs().isEmpty();
}

} // namespace

QnResourceList DesktopAudioOnlyResourceSearcherImpl::findResources()
{
    QnResourceList result;

    if (hasAudioInputs())
        result << QnResourcePtr(new DesktopAudioOnlyResource());

    return result;
}

} // namespace nx::vms::client::core
