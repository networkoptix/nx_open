// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::client::core {
namespace ptz {

static constexpr int kNoHotkey = -1;

using PresetIdByHotkey = QHash<int, QString>;

} // namespace ptz
} // namespace nx::vms::client::core
