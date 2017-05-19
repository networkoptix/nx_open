#pragma once

class QCheckBox;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

struct CheckboxUtils
{
    static void autoClearTristate(QCheckBox* checkbox);
    static void setupTristateCheckbox(QCheckBox* checkbox, bool sameValue, bool checked);
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
