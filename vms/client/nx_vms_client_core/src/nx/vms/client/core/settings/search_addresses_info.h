// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QSet>

#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

using ServerSearchAddressesHash = QHash<QnUuid, QSet<QString>>;
using SystemSearchAddressesHash = QHash<QnUuid, ServerSearchAddressesHash>;

} // namespace nx::vms::client::core
