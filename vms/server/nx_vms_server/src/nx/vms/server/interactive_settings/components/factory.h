#pragma once

#include "item.h"

namespace nx::vms::server::interactive_settings::components {

class Factory
{
public:
    static void registerTypes();
    static Item* createItem(const QString &type, QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
