#include "check_box.h"

namespace nx::vms::server::interactive_settings::components {

CheckBox::CheckBox(QObject* parent):
    BooleanValueItem(QStringLiteral("CheckBox"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
