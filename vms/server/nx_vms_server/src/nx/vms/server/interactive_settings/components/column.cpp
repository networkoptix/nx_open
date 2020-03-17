#include "column.h"

namespace nx::vms::server::interactive_settings::components {

Column::Column(QObject* parent):
    Group(QStringLIteral("Column"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
