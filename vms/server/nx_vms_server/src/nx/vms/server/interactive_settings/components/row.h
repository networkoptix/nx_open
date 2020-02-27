#pragma once

#include "group.h"

namespace nx::vms::server::interactive_settings::components {

class Row: public Group
{
public:
    Row(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
