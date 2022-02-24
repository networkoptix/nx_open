// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/widgets/detail/base_input_field.h>

namespace nx::vms::client::desktop {

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

} // namespace nx::vms::client::desktop
