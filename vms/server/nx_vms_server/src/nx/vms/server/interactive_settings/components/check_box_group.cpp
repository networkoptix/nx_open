#include "check_box_group.h"

namespace nx::vms::server::interactive_settings::components {

CheckBoxGroup::CheckBoxGroup(QObject* parent):
    StringArrayValueItem(QStringLiteral("CheckBoxGroup"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
