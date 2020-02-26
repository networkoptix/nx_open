#pragma once

#include "enumeration_item.h"

namespace nx::vms::server::interactive_settings::components {

class ComboBox: public EnumerationItem
{
public:
    ComboBox(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
