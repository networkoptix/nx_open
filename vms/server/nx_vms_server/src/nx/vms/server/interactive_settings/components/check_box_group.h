#pragma once

#include "enumeration_item.h"

namespace nx::vms::server::interactive_settings::components {

class CheckBoxGroup: public EnumerationItem
{
public:
    CheckBoxGroup(QObject* parent = nullptr);

    virtual void setValue(const QVariant& value);
};

} // namespace nx::vms::server::interactive_settings::components
