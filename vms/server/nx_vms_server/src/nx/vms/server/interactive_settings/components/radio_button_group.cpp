#include "radio_button_group.h"

namespace nx::vms::server::interactive_settings::components {

RadioButtonGroup::RadioButtonGroup(QObject* parent):
    EnumerationValueItem(QStringLiteral("RadioButtonGroup"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
