// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_audio_only_resource_searcher_impl.h"

#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QMediaDevices>

#include "desktop_audio_only_resource.h"

namespace nx::vms::client::core {

QnResourceList DesktopAudioOnlyResourceSearcherImpl::findResources()
{
    QnResourceList result;
    const auto availableDevices = QMediaDevices::audioInputs();

    if (!availableDevices.isEmpty())
        result << QnResourcePtr(new DesktopAudioOnlyResource());

    return result;
}

} // namespace nx::vms::client::core
