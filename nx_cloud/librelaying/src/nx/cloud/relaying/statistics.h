#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace cloud {
namespace relaying {

struct Statistics
{
    int listeningServerCount = 0;
    int connectionsCount = 0;
    int connectionsAveragePerServerCount = 0;
    int connectionsAcceptedPerMinute = 0;
};

#define Statistics_relaying_Fields (listeningServerCount)(connectionsCount) \
    (connectionsAveragePerServerCount)(connectionsAcceptedPerMinute)

bool NX_RELAYING_API deserialize(QnJsonContext*, const QJsonValue&, Statistics*);
void NX_RELAYING_API serialize(QnJsonContext*, const Statistics&, class QJsonValue*);

} // namespace relaying
} // namespace cloud
} // namespace nx
