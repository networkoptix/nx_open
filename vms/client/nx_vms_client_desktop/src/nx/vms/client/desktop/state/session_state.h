// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QMap>
#include <QtCore/QString>

namespace nx::vms::client::desktop {

using DelegateState = QMap<QString, QJsonValue>;
using SessionState = QMap<QString, DelegateState>;
using FullSessionState = std::map<QString, SessionState>;

} // namespace nx::vms::client::desktop
