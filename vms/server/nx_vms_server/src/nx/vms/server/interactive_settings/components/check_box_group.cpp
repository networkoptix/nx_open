#include "check_box_group.h"

namespace nx::vms::server::interactive_settings::components {

CheckBoxGroup::CheckBoxGroup(QObject* parent):
    EnumerationItem(QStringLiteral("CheckBoxGroup"), parent)
{
}

void CheckBoxGroup::setValue(const QVariant& value)
{
    const auto validOptions = range();
    QVariantList effectiveValue;

    for (const auto& option: value.value<QVariantList>())
    {
        if (validOptions.contains(option))
            effectiveValue.push_back(option);
    }

    ValueItem::setValue(effectiveValue);
}

} // namespace nx::vms::server::interactive_settings::components
