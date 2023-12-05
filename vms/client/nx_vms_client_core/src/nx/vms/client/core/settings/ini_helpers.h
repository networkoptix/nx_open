// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVariant>

namespace nx::kit { class IniConfig;}

namespace nx::vms::client::core {

NX_VMS_CLIENT_CORE_API QVariant getIniValue(const nx::kit::IniConfig& config, const QString& name);

} // namespace nx::vms::client::core
