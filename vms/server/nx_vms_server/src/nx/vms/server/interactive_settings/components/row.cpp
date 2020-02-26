#include "row.h"

namespace nx::vms::server::interactive_settings::components {

Row::Row(QObject* parent):
    Group(QStringLiteral("Row"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
