// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_sdk_object_detected_widget.h"
#include "ui_analytics_sdk_object_detected_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QSortFilterProxyModel>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {
namespace ui {

AnalyticsSdkObjectDetectedWidget::AnalyticsSdkObjectDetectedWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::AnalyticsSdkObjectDetectedWidget)
{
    ui->setupUi(this);

    connect(ui->attributesEdit, &QLineEdit::textChanged, this,
        &AnalyticsSdkObjectDetectedWidget::paramsChanged);
    connect(ui->detectableObjectTypeComboBox, QnComboboxCurrentIndexChanged, this,
        &AnalyticsSdkObjectDetectedWidget::paramsChanged);

    ui->sdkObjectTypeLabel->addHintLine(tr("Analytics object detection can be set up on a certain cameras."));
    ui->sdkObjectTypeLabel->addHintLine(tr("Choose cameras using the button above to see the list "
        "of supported events."));
    setHelpTopic(ui->sdkObjectTypeLabel, HelpTopic::Id::EventsActions_VideoAnalytics);

    ui->attributesLabel->addHintLine(tr("Event will trigger only if there are matches any of attributes. You can see the names of the attributes and their values on the Objects tab."));
    setHelpTopic(ui->attributesEdit, HelpTopic::Id::EventsActions_VideoAnalytics);
}

AnalyticsSdkObjectDetectedWidget::~AnalyticsSdkObjectDetectedWidget()
{
}

void AnalyticsSdkObjectDetectedWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->detectableObjectTypeComboBox);
    setTabOrder(ui->detectableObjectTypeComboBox, ui->attributesEdit);
    setTabOrder(ui->attributesEdit, after);
}

void AnalyticsSdkObjectDetectedWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;
    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::eventParams))
    {
        QString attributes = model()->eventParams().description;
        if (ui->attributesEdit->text() != attributes)
            ui->attributesEdit->setText(attributes);
    }

    if (fields.testFlag(Field::eventResources))
        ui->detectableObjectTypeComboBox->setDevices(model()->eventResources());

    ui->detectableObjectTypeComboBox->setSelectedMainObjectTypeId(
        model()->eventParams().getAnalyticsObjectTypeId());

    // Ensure the model params match the UI.
    model()->setEventParams(createEventParameters(
        ui->detectableObjectTypeComboBox->selectedMainObjectTypeId(), ui->attributesEdit->text()));
}

void AnalyticsSdkObjectDetectedWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;
    QScopedValueRollback<bool> guard(m_updating, true);

    model()->setEventParams(createEventParameters(
        ui->detectableObjectTypeComboBox->selectedMainObjectTypeId(), ui->attributesEdit->text()));
}

nx::vms::event::EventParameters AnalyticsSdkObjectDetectedWidget::createEventParameters(
    const QString& analyticsObjectTypeId, const QString& attributes)
{
    auto eventParams = model()->eventParams();
    eventParams.setAnalyticsObjectTypeId(analyticsObjectTypeId);
    eventParams.description = attributes;
    return eventParams;
}

} // namespace ui
} // namespace nx::vms::client::desktop
