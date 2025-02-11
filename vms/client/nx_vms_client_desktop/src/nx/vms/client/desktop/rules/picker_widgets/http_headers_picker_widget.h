// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QPushButton>

#include <nx/vms/rules/action_builder_fields/http_headers_field.h>

#include "field_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class HttpHeadersPickerWidget: public PlainFieldPickerWidget<vms::rules::HttpHeadersField>
{
    Q_OBJECT

public:
    HttpHeadersPickerWidget(
        vms::rules::HttpHeadersField* field,
        SystemContext* context,
        ParamsWidget* parent);

protected:
    void updateUi() override;

private:
    QPushButton* m_button{nullptr};

    void onButtonClicked();
};

} // namespace nx::vms::client::desktop::rules
