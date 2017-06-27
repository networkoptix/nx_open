#pragma once

#include <nx/client/desktop/ui/common/detail/base_input_field.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class TextEditField: public detail::BaseInputField
{
    Q_OBJECT
    using base_type = detail::BaseInputField;

public:
    TextEditField(QWidget* parent = nullptr);
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
