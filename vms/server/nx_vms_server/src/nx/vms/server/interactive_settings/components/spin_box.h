#pragma once

#include "numeric_items.h"

namespace nx::vms::server::interactive_settings::components {

class SpinBox: public IntegerNumberItem
{
public:
    SpinBox(QObject* parent = nullptr);
};

class DoubleSpinBox: public RealNumberItem
{
public:
    DoubleSpinBox(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
