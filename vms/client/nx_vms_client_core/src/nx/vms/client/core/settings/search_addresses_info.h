// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>
#include <QtCore/QHash>
#include <nx/utils/uuid.h>

using ServerSearchAddressesHash = QHash<QnUuid, QSet<QString>>;
using SystemSearchAddressesHash = QHash<QnUuid, ServerSearchAddressesHash>;
