// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QSize>

namespace nx::vms::client::core {

// This facade class must be used instead of nx::media::DecoderRegistrar as a workaround to avoid
// VideoDecoderRegistry singleton instance duplication in different Windows modules
// (i.e. nx_vms_client_desktop.dll & nx_vms_client_core.dll)
// When/if nx_media becomes a dynamic library (currently it's static), this problem will be solved.
class NX_VMS_CLIENT_CORE_API DecoderRegistrar
{
public:
    static void registerDecoders(const QMap<int, QSize>& maxFfmpegResolutions);
};

} // namespace nx::vms::client::core
