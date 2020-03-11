#pragma once

#include "enumeration_value_item.h"

namespace nx::vms::server::interactive_settings::components {

class RadioButtonGroup: public EnumerationValueItem
{
public:
    RadioButtonGroup(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
