#pragma once

#include "group.h"

namespace nx::vms::server::interactive_settings::components {

class GroupBox: public Group
{
public:
    GroupBox(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
