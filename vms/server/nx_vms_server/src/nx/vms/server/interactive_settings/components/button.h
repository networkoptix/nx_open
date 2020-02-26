#pragma once

#include "item.h"

namespace nx::vms::server::interactive_settings::components {

class Button: public Item
{
public:
    Button(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
