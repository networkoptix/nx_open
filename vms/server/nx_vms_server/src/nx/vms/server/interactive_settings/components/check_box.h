#pragma once

#include "value_item.h"

namespace nx::vms::server::interactive_settings::components {

class CheckBox: public ValueItem
{
    Q_OBJECT
    Q_PROPERTY(bool value READ value WRITE setValue NOTIFY valueChanged FINAL)

public:
    CheckBox(QObject* parent = nullptr);

    bool value() const
    {
        return ValueItem::value().toBool();
    }

    void setValue(bool value)
    {
        ValueItem::setValue(value);
    }

    using ValueItem::setValue;
};

} // namespace nx::vms::server::interactive_settings::components
