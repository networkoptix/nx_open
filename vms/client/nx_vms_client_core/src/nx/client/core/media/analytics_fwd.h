#pragma once

#include <QtCore/QSharedPointer>

namespace nx::vms::client::core {

class AbstractAnalyticsMetadataProvider;
using AbstractAnalyticsMetadataProviderPtr = QSharedPointer<AbstractAnalyticsMetadataProvider>;

} // namespace nx::vms::client::core
