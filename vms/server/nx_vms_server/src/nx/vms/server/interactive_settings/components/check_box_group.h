#pragma once

#include "string_array_value_item.h"

namespace nx::vms::server::interactive_settings::components {

class CheckBoxGroup: public StringArrayValueItem
{
public:
    CheckBoxGroup(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
