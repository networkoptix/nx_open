#pragma once

#include <nx/utils/std/optional.h>

class QSpinBox;

namespace nx::vms::client::desktop {
namespace spin_box_utils {

void autoClearSpecialValue(QSpinBox* spinBox, int startingValue);

void setupSpinBox(QSpinBox* spinBox, bool sameValue, int value);
void setupSpinBox(QSpinBox* spinBox, std::optional<int> value);
void setupSpinBox(QSpinBox* spinBox, bool sameValue, int value, int min, int max);
void setupSpinBox(QSpinBox* spinBox, std::optional<int> value, int min, int max);

std::optional<int> value(QSpinBox* spinBox);

void setSpecialValue(QSpinBox* spinBox);
bool isSpecialValue(QSpinBox* spinBox);

} // namespace spin_box_utils
} // namespace nx::vms::client::desktop
