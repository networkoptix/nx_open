#pragma once

#include "string_value_item.h"

namespace nx::vms::server::interactive_settings::components {

class TextArea: public StringValueItem
{
public:
    TextArea(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
