// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QJsonObject>

namespace nx::vms::api::analytics {

using SettingsModel = QJsonObject;
using SettingsValues = QJsonObject;
using SettingsErrors = QMap<QString, QString>;

} // namespace nx::vms::api::analytics
