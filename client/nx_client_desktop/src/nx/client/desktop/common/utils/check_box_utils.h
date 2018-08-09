#pragma once

#include <nx/utils/std/optional.h>

class QCheckBox;

namespace nx {
namespace client {
namespace desktop {
namespace check_box_utils {

void autoClearTristate(QCheckBox* checkbox);
void setupTristateCheckbox(QCheckBox* checkbox, bool sameValue, bool checked);
void setupTristateCheckbox(QCheckBox* checkbox, std::optional<bool> checked);

} // namespace check_box_utils
} // namespace desktop
} // namespace client
} // namespace nx
