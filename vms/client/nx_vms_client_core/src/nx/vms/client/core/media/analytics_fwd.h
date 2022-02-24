// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

namespace nx::vms::client::core {

class AbstractAnalyticsMetadataProvider;
using AbstractAnalyticsMetadataProviderPtr = QSharedPointer<AbstractAnalyticsMetadataProvider>;

} // namespace nx::vms::client::core
