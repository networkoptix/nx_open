#pragma once

#include "boolean_value_item.h"

namespace nx::vms::server::interactive_settings::components {

class SwitchButton: public BooleanValueItem
{
public:
    SwitchButton(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
