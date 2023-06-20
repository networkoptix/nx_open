// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QPixmap>

#include <core/resource/resource_fwd.h>
#include <nx/vms/event/level.h>
#include <nx/vms/rules/icon.h>

namespace nx::vms::client::desktop {

bool needIconDevices(nx::vms::rules::Icon icon);

QPixmap eventIcon(
    nx::vms::rules::Icon icon,
    nx::vms::event::Level level,
    const QString& custom,
    const QColor& color,
    const QnResourceList& devices);

} // namespace nx::vms::client::desktop
