// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QPushButton>

#include <nx/vms/rules/action_builder_fields/integration_action_field.h>
#include <nx/vms/rules/action_builder_fields/integration_action_parameters_field.h>

#include "field_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class IntegrationActionParametersPickerWidget:
    public PlainFieldPickerWidget<vms::rules::IntegrationActionParametersField>
{
    Q_OBJECT

public:
    IntegrationActionParametersPickerWidget(
        vms::rules::IntegrationActionParametersField* field,
        SystemContext* context,
        ParamsWidget* parent);

protected:
    void updateUi() override;

private:
    QPushButton* m_button{nullptr};

    void onButtonClicked();
    QJsonObject getSettingsModel();
};

} // namespace nx::vms::client::desktop::rules
