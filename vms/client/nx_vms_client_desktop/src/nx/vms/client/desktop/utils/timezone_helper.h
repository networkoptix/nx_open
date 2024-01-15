// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QTimeZone>

#include <core/resource/resource_fwd.h>
#include <nx/vms/time/timezone.h>

namespace nx::vms::client::desktop {

/** Actual timezone of the resource. */
QTimeZone timeZone(const QnMediaResourcePtr& mediaResource);

/** Timezone of the resource concerning current desktop client time mode. */
QTimeZone displayTimeZone(const QnMediaResourcePtr& mediaResource);

} // namespace nx::vms::client::desktop
