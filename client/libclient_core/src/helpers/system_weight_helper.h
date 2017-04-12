#pragma once

#include <QtCore/qglobal.h>

#include <utils/common/id.h>

namespace nx {
namespace client {
namespace core {
namespace helpers {

qreal calculateSystemWeight(qreal baseWeight, qint64 lastConnectedUtcMs);
void updateWeightData(const QnUuid& localId);

} // helpers namespace
} // namespace core
} // namespace client
} // namespace nx
