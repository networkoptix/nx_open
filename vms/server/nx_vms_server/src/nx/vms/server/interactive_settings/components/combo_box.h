#pragma once

#include "enumeration_value_item.h"

namespace nx::vms::server::interactive_settings::components {

class ComboBox: public EnumerationValueItem
{
public:
    ComboBox(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
