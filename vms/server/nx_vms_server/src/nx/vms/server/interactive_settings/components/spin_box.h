#pragma once

#include "numeric_value_items.h"

namespace nx::vms::server::interactive_settings::components {

class SpinBox: public IntegerValueItem
{
public:
    SpinBox(QObject* parent = nullptr);
};

class DoubleSpinBox: public RealValueItem
{
public:
    DoubleSpinBox(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
