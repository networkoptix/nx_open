#include "switch_button.h"

namespace nx::vms::server::interactive_settings::components {

SwitchButton::SwitchButton(QObject* parent):
    BooleanValueItem(QStringLiteral("SwitchButton"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
