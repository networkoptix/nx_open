#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace cloud {
namespace relaying {

struct NX_RELAYING_API Statistics
{
    int listeningServerCount = 0;
    int connectionCount = 0;
    int connectionsAveragePerServerCount = 0;
    int connectionsAcceptedPerMinute = 0;

    bool operator==(const Statistics& right) const;
};

#define Statistics_relaying_Fields (listeningServerCount)(connectionCount) \
    (connectionsAveragePerServerCount)(connectionsAcceptedPerMinute)

bool NX_RELAYING_API deserialize(QnJsonContext*, const QJsonValue&, Statistics*);
void NX_RELAYING_API serialize(QnJsonContext*, const Statistics&, class QJsonValue*);

} // namespace relaying
} // namespace cloud
} // namespace nx
