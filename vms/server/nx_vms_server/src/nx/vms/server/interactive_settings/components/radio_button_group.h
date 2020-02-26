#pragma once

#include "enumeration_item.h"

namespace nx::vms::server::interactive_settings::components {

class RadioButtonGroup: public EnumerationItem
{
public:
    RadioButtonGroup(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
