#pragma once

#include <nx/utils/std/optional.h>

class QSpinBox;

namespace nx {
namespace client {
namespace desktop {

struct SpinBoxUtils
{
    static void autoClearSpecialValue(QSpinBox* spinBox, int startingValue);

    static void setupSpinBox(QSpinBox* spinBox, bool sameValue, int value);
    static void setupSpinBox(QSpinBox* spinBox, std::optional<int> value);
    static void setupSpinBox(QSpinBox* spinBox, bool sameValue, int value, int min, int max);
    static void setupSpinBox(QSpinBox* spinBox, std::optional<int> value, int min, int max);

    static std::optional<int> value(QSpinBox* spinBox);

    static void setSpecialValue(QSpinBox* spinBox);
    static bool isSpecialValue(QSpinBox* spinBox);
};

} // namespace desktop
} // namespace client
} // namespace nx
