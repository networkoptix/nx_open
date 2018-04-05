#pragma once

#include <nx/client/desktop/common/widgets/detail/base_input_field.h>

namespace nx {
namespace client {
namespace desktop {

class TextEditField: public detail::BaseInputField
{
    Q_OBJECT
    using base_type = detail::BaseInputField;

public:
    static TextEditField* create(
        const QString& text,
        const TextValidateFunction& validator,
        QWidget* parent = nullptr);

    TextEditField(QWidget* parent = nullptr);
};

} // namespace desktop
} // namespace client
} // namespace nx
