#include "spin_box.h"

namespace nx::vms::server::interactive_settings::components {

SpinBox::SpinBox(QObject* parent):
    IntegerValueItem(QStringLiteral("SpinBox"), parent)
{
}

DoubleSpinBox::DoubleSpinBox(QObject* parent):
    RealValueItem(QStringLiteral("DoubleSpinBox"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
