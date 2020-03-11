#pragma once

#include "item.h"

namespace nx::vms::server::interactive_settings::components {

/** A visual separator between items. */
class Separator: public Item
{
    Q_OBJECT

public:
    Separator(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
