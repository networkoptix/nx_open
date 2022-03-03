// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>

namespace nx::vms::rules {

QMap<QString, QJsonValue> serializeProperties(const QObject* object);

void deserializeProperties(const QMap<QString, QJsonValue>& propMap, QObject* object);

} // namespace nx::vms::rules
