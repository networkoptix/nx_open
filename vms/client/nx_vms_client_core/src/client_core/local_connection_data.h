#pragma once

#include <QtCore/QUrl>

#include <nx/fusion/model_functions_fwd.h>
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
    bool realConnection; //< Shows if it was real connection or just record for new system
};
#define WeightData_Fields (localId)(weight)(lastConnectedUtcMs)(realConnection)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (LocalConnectionData)(WeightData), (eq)(json))

} // namespace nx::vms::client::core

Q_DECLARE_METATYPE(nx::vms::client::core::LocalConnectionData)
Q_DECLARE_METATYPE(nx::vms::client::core::WeightData)
