#pragma once

#include "value_item.h"

namespace nx::vms::server::interactive_settings::components {

class StringValueItem: public ValueItem
{
public:
    using ValueItem::ValueItem;

    virtual QJsonValue normalizedValue(const QJsonValue& value) const override;
    virtual QJsonValue fallbackDefaultValue() const override;
};

} // namespace nx::vms::server::interactive_settings::components
