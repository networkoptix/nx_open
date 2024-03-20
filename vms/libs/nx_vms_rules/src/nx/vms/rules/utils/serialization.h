// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>

namespace nx::vms::rules {

NX_VMS_RULES_API QMap<QString, QJsonValue> serializeProperties(
    const QObject* object,
    const QSet<QByteArray>& names);

NX_VMS_RULES_API bool deserializeProperties(
    const QMap<QString, QJsonValue>& propMap,
    QObject* object);

NX_VMS_RULES_API QByteArray serialized(const QObject* object);

} // namespace nx::vms::rules
