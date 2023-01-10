// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

namespace nx::vms::client::core {

struct LocalConnectionData
{
    QString systemName;
    QList<nx::utils::Url> urls;
};
#define LocalConnectionData_Fields (systemName)(urls)

struct WeightData
{
    QnUuid localId;
    qreal weight;
    qint64 lastConnectedUtcMs;
    bool realConnection; //< Shows if it was real connection or just record for new system.

    bool operator==(const WeightData& other) const;
};
#define WeightData_Fields (localId)(weight)(lastConnectedUtcMs)(realConnection)
NX_REFLECTION_INSTRUMENT(WeightData, WeightData_Fields)

QN_FUSION_DECLARE_FUNCTIONS(LocalConnectionData, (json))
QN_FUSION_DECLARE_FUNCTIONS(WeightData, (json))

} // namespace nx::vms::client::core
