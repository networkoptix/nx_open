#pragma once

#include "boolean_value_item.h"

namespace nx::vms::server::interactive_settings::components {

class CheckBox: public BooleanValueItem
{
public:
    CheckBox(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
