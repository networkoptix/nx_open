#pragma once

#include <nx/utils/std/optional.h>

class QCheckBox;

namespace nx {
namespace client {
namespace desktop {

struct CheckboxUtils
{
    static void autoClearTristate(QCheckBox* checkbox);
    static void setupTristateCheckbox(QCheckBox* checkbox, bool sameValue, bool checked);
    static void setupTristateCheckbox(QCheckBox* checkbox, std::optional<bool> checkState);
};

} // namespace desktop
} // namespace client
} // namespace nx
