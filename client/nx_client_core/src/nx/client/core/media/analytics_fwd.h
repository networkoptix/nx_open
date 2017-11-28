#pragma once

#include <QtCore/QSharedPointer>

namespace nx {
namespace client {
namespace core {

class AbstractAnalyticsMetadataProvider;
using AbstractAnalyticsMetadataProviderPtr = QSharedPointer<AbstractAnalyticsMetadataProvider>;

} // namespace core
} // namespace client
} // namespace nx
