// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/qglobal.h>

#include <utils/common/id.h>

namespace nx::vms::client::core {
namespace helpers {

qreal calculateSystemWeight(qreal baseWeight, qint64 lastConnectedUtcMs);
void updateWeightData(const QnUuid& localId);

} // helpers namespace
} // namespace nx::vms::client::core
