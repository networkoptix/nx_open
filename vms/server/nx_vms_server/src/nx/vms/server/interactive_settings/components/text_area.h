#pragma once

#include "value_item.h"

namespace nx::vms::server::interactive_settings::components {

class TextArea: public ValueItem
{
public:
    TextArea(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
