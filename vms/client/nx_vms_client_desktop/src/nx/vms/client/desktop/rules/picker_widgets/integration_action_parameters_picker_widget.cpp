// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_action_parameters_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <core/resource_management/resource_pool.h>
#include <nx/analytics/utils.h>
#include <nx/vms/api/analytics/object_action.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/ui/dialogs/analytics_action_settings_dialog.h>
#include <nx/vms/rules/utils/field_names.h>

namespace nx::vms::client::desktop::rules {

IntegrationActionParametersPickerWidget::IntegrationActionParametersPickerWidget(
    vms::rules::IntegrationActionParametersField* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    PlainFieldPickerWidget<vms::rules::IntegrationActionParametersField>(field, context, parent)
{
    auto contentLayout = new QHBoxLayout{m_contentWidget};

    m_button = new QPushButton;
    m_button->setProperty(
        style::Properties::kPushButtonMargin, style::Metrics::kStandardPadding);
    contentLayout->addWidget(m_button);

    connect(
        m_button,
        &QPushButton::clicked,
        this,
        &IntegrationActionParametersPickerWidget::onButtonClicked);
}

QJsonObject IntegrationActionParametersPickerWidget::getSettingsModel()
{
    const QString integrationAction = getActionField<vms::rules::IntegrationActionField>(
        nx::vms::rules::utils::kIntegrationActionFieldName)->value();
    const auto engines = resourcePool()->getResources<vms::common::AnalyticsEngineResource>();
    const std::optional<QJsonObject> settingsModel =
        nx::analytics::findSettingsModelByIntegrationActionId(integrationAction, engines);

    if (!settingsModel)
    {
        NX_ERROR(this, "Settings model not found for Integration Action %1", integrationAction);
        return QJsonObject();
    }

    return *settingsModel;
}

void IntegrationActionParametersPickerWidget::updateUi()
{
    PlainFieldPickerWidget<vms::rules::IntegrationActionParametersField>::updateUi();

    const std::optional<QJsonObject> settingsModel = getSettingsModel();
    if (settingsModel->empty())
    {
        m_button->setEnabled(false);
        m_button->setText(tr("No settings model"));
        return;
    }

    m_button->setEnabled(true);
    m_button->setText(m_field->value() == "" ? tr("No parameters") : tr("Parameters set"));
}

void IntegrationActionParametersPickerWidget::onButtonClicked()
{
    const QJsonObject settingsModel = getSettingsModel();
    if (settingsModel.empty())
        return;

    QEventLoop loop;
    AnalyticsActionSettingsDialog::request(
        settingsModel,
        [this, &loop](std::optional<QJsonObject> settingsJson)
        {
            if (settingsJson)
                m_field->setValue(settingsJson.value());

            loop.quit();
        },
        m_field->value(),
        parentParamsWidget());

    loop.exec();
}

} // namespace nx::vms::client::desktop::rules
