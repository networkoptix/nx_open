#include "group_box.h"

namespace nx::vms::server::interactive_settings::components {

GroupBox::GroupBox(QObject* parent):
    Group(QStringLiteral("GroupBox"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
