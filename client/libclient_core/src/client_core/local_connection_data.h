#pragma once

#include <QtCore/QUrl>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

#include <utils/crypt/encoded_string.h>

namespace nx {
namespace client {
namespace core {

struct LocalConnectionData
{
    QString systemName;
    QList<QUrl> urls;
};
#define LocalConnectionData_Fields (systemName)(urls)

struct SingleConnectionData
{
    QString systemName;
    QUrl url;
};
#define SingleConnectionData_Fields (systemName)(url)

struct WeightData
{
    QnUuid localId;
    qreal weight;
    qint64 lastConnectedUtcMs;
    bool realConnection; //< Shows if it was real connection or just record for new system
};
#define WeightData_Fields (localId)(weight)(lastConnectedUtcMs)(realConnection)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (LocalConnectionData)(SingleConnectionData)(WeightData), (eq)(json))

} // namespace core
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::core::LocalConnectionData)
Q_DECLARE_METATYPE(nx::client::core::SingleConnectionData)
Q_DECLARE_METATYPE(nx::client::core::WeightData)
