// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/vms/api/types/resource_types.h>

namespace nx::vms::client::desktop {

QString toDisplayString(nx::vms::api::StreamQuality value);
QString toShortDisplayString(nx::vms::api::StreamQuality value);

} // namespace nx::vms::client::desktop
