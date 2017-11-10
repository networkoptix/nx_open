#pragma once

#include <QtCore/QSharedPointer>

namespace nx {
namespace client {
namespace desktop {

class AbstractAnalyticsDriver;
using AbstractAnalyticsDriverPtr = QSharedPointer<AbstractAnalyticsDriver>;

} // namespace desktop
} // namespace client
} // namespace nx
