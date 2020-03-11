#pragma once

#include "group.h"

namespace nx::vms::server::interactive_settings::components {

/** A grouping item which arranges items in a row (horizontally). */
class Row: public Group
{
public:
    Row(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
