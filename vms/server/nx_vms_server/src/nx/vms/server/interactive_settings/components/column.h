#pragma once

#include "group.h"

namespace nx::vms::server::interactive_settings::components {

/** A grouping item which arranges items in a column (vertically). */
class Column: public Group
{
public:
    Column(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
