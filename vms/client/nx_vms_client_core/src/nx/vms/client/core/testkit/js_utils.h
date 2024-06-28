// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtQml/QJSValue>

namespace nx::vms::client::core::testkit {

NX_VMS_CLIENT_CORE_API QJsonObject makeResponse(const QJSValue& result);
NX_VMS_CLIENT_CORE_API QJsonObject makeErrorResponse(const QString& message);

} // namespace nx::vms::client::core::testkit
