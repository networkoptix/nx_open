// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timezone_helper.h"

#include <core/resource/avi/avi_resource.h>
#include <nx/vms/client/core/resource/server.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>

namespace nx::vms::client::desktop {

// FIXME: #sivanov Code duplication with core::ServerTimeWatcher.
QTimeZone timeZone(const QnMediaResourcePtr& mediaResource)
{
    const auto resource = mediaResource->toResourcePtr();

    if (auto fileResource = resource.dynamicCast<QnAviResource>())
        return fileResource->timeZone();

    if (auto server = resource->getParentResource().dynamicCast<core::ServerResource>())
        return server->timeZone();

    return QTimeZone{QTimeZone::LocalTime};
}

QTimeZone displayTimeZone(const QnMediaResourcePtr& mediaResource)
{
    if (appContext()->localSettings()->timeMode() == Qn::ClientTimeMode)
        return QTimeZone::LocalTime;

    return timeZone(mediaResource);
}

} // namespace nx::vms::client::desktop
