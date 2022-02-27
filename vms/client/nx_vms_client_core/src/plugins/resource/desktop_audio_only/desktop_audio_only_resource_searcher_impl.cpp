// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_audio_only_resource_searcher_impl.h"

#include <QtMultimedia/QAudioDeviceInfo>

#include <plugins/resource/desktop_audio_only/desktop_audio_only_resource.h>

QnResourceList QnDesktopAudioOnlyResourceSearcherImpl::findResources()
{
    QnResourceList result;
    const auto availableDevices =
        QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

    if (!availableDevices.isEmpty())
        result << QnResourcePtr(new QnDesktopAudioOnlyResource());

    return result;
}
