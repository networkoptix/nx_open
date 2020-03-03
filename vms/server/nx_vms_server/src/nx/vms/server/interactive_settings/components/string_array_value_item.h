#pragma once

#include "enumeration_value_item.h"

namespace nx::vms::server::interactive_settings::components {

class StringArrayValueItem: public EnumerationValueItem
{
public:
    using EnumerationValueItem::EnumerationValueItem;

    virtual void applyConstraints() override;
    virtual QJsonValue normalizedValue(const QJsonValue& value) const override;
    virtual QJsonValue fallbackDefaultValue() const override;
};

} // namespace nx::vms::server::interactive_settings::components
