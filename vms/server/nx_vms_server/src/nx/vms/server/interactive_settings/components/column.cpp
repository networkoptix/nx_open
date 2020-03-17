#include "column.h"

namespace nx::vms::server::interactive_settings::components {

Column::Column(QObject* parent):
    Group(QStringLiteral("Column"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
