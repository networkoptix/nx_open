#include "spin_box.h"

namespace nx::vms::server::interactive_settings::components {

SpinBox::SpinBox(QObject* parent):
    IntegerNumberItem(QStringLiteral("SpinBox"), parent)
{
}

DoubleSpinBox::DoubleSpinBox(QObject* parent):
    RealNumberItem(QStringLiteral("DoubleSpinBox"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
