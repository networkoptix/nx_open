// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/settings/welcome_screen_info.h>

namespace nx::vms::client::core {

struct SystemVisibilityScopeInfo
{
    QString name;
    welcome_screen::TileVisibilityScope visibilityScope;

    bool operator==(const SystemVisibilityScopeInfo&) const = default;
};
NX_REFLECTION_INSTRUMENT(SystemVisibilityScopeInfo, (name)(visibilityScope))

using SystemVisibilityScopeInfoHash = QHash<nx::Uuid, SystemVisibilityScopeInfo>;

} // namespace nx::vms::client::core
