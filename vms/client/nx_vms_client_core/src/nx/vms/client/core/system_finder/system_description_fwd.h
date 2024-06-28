// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

namespace nx::vms::client::core {

class SystemDescription;
using SystemDescriptionPtr = QSharedPointer<SystemDescription>;
using SystemDescriptionList = QList<SystemDescriptionPtr>;

} // namespace nx::vms::client::core
