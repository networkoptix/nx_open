// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/software_version.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

struct LocalConnectionData
{
    QString systemName;
    QList<nx::utils::Url> urls;
    nx::utils::SoftwareVersion version;

    bool operator==(const LocalConnectionData&) const = default;
};
NX_REFLECTION_INSTRUMENT(LocalConnectionData, (systemName)(urls))

struct NX_VMS_CLIENT_CORE_API WeightData
{
    QnUuid localId;
    qreal weight;
    qint64 lastConnectedUtcMs;
    bool realConnection; //< Shows if it was real connection or just record for new system.

    bool operator==(const WeightData& other) const;
};
NX_REFLECTION_INSTRUMENT(WeightData, (localId)(weight)(lastConnectedUtcMs)(realConnection))

} // namespace nx::vms::client::core
