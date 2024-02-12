// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QSet>

#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

using ServerSearchAddressesHash = QHash<nx::Uuid, QSet<QString>>;
using SystemSearchAddressesHash = QHash<nx::Uuid, ServerSearchAddressesHash>;

} // namespace nx::vms::client::core
