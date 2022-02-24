// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/resource_property_adaptor.h>

#include "types.h"

namespace nx::vms::client::core {
namespace ptz {

class HotkeysResourcePropertyAdaptor: public QnJsonResourcePropertyAdaptor<PresetIdByHotkey>
{
    Q_OBJECT
    using base_type = QnJsonResourcePropertyAdaptor<PresetIdByHotkey>;

public:
    HotkeysResourcePropertyAdaptor(QObject* parent = nullptr);
};

} // namespace ptz
} // namespace nx::vms::client::core
