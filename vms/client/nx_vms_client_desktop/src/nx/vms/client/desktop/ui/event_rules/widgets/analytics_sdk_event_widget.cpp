// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_sdk_event_widget.h"
#include "ui_analytics_sdk_event_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QSortFilterProxyModel>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ui/event_rules/models/analytics_sdk_event_model.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {
namespace ui {

AnalyticsSdkEventWidget::AnalyticsSdkEventWidget(SystemContext* systemContext, QWidget* parent):
    base_type(systemContext, parent),
    ui(new Ui::AnalyticsSdkEventWidget),
    m_sdkEventModel(new AnalyticsSdkEventModel(systemContext, this))
{
    ui->setupUi(this);

    auto sortModel = new QSortFilterProxyModel(this);
    sortModel->setDynamicSortFilter(true);
    sortModel->setSourceModel(m_sdkEventModel);
    ui->sdkEventTypeComboBox->setModel(sortModel);

    connect(ui->captionEdit, &QLineEdit::textChanged, this,
        &AnalyticsSdkEventWidget::paramsChanged);
    connect(ui->descriptionEdit, &QLineEdit::textChanged, this,
        &AnalyticsSdkEventWidget::paramsChanged);
    connect(ui->sdkEventTypeComboBox, QnComboboxCurrentIndexChanged, this,
        &AnalyticsSdkEventWidget::paramsChanged);

    ui->sdkEventTypeLabel->addHintLine(tr("Analytics events can be set up on a certain cameras."));
    ui->sdkEventTypeLabel->addHintLine(tr("Choose cameras using the button above to see the list "
        "of supported events."));
    setHelpTopic(ui->sdkEventTypeLabel, HelpTopic::Id::EventsActions_VideoAnalytics);

    ui->captionLabel->addHintLine(tr("Event will trigger only if there are matches in the caption "
        "with any of the entered keywords."));
    ui->captionLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    ui->captionLabel->addHintLine(tr("This field is case sensitive."));
    setHelpTopic(ui->captionLabel, HelpTopic::Id::EventsActions_VideoAnalytics);

    ui->descriptionLabel->addHintLine(tr("Event will trigger only if there are matches in the "
        "description field with any of the entered keywords."));
    ui->descriptionLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    ui->descriptionLabel->addHintLine(tr("This field is case sensitive."));
    setHelpTopic(ui->descriptionLabel, HelpTopic::Id::EventsActions_VideoAnalytics);
}

AnalyticsSdkEventWidget::~AnalyticsSdkEventWidget()
{
}

void AnalyticsSdkEventWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->captionEdit);
    setTabOrder(ui->captionEdit, ui->descriptionEdit);
    setTabOrder(ui->descriptionEdit, after);
}

void AnalyticsSdkEventWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;
    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::eventResources))
        updateSdkEventTypesModel();

    if (fields.testFlag(Field::eventParams))
    {
        QString caption = model()->eventParams().caption;
        if (ui->captionEdit->text() != caption)
            ui->captionEdit->setText(caption);

        QString description = model()->eventParams().description;
        if (ui->descriptionEdit->text() != description)
            ui->descriptionEdit->setText(description);
    }

    if (fields.testFlag(Field::eventResources) || fields.testFlag(Field::eventParams))
        updateSelectedEventType();
}

void AnalyticsSdkEventWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;
    QScopedValueRollback<bool> guard(m_updating, true);

    model()->setEventParams(createEventParameters(
        ui->sdkEventTypeComboBox->currentData(
            AnalyticsSdkEventModel::EngineIdRole).value<QnUuid>(),
        ui->sdkEventTypeComboBox->currentData(
            AnalyticsSdkEventModel::EventTypeIdRole).value<QString>()));
}

void AnalyticsSdkEventWidget::updateSdkEventTypesModel()
{
    const auto cameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
        model()->eventResources());

    m_sdkEventModel->loadFromCameras(cameras, model()->eventParams());
    ui->sdkEventTypeComboBox->setEnabled(m_sdkEventModel->isValid());
    ui->sdkEventTypeComboBox->model()->sort(0);
}

void AnalyticsSdkEventWidget::updateSelectedEventType()
{
    QnUuid engineId = model()->eventParams().getAnalyticsEngineId();
    QString eventTypeId = model()->eventParams().getAnalyticsEventTypeId();

    auto analyticsModel = ui->sdkEventTypeComboBox->model();

    if (engineId.isNull() || eventTypeId.isNull())
    {
        const auto selectableItems = analyticsModel->match(
            analyticsModel->index(0, 0),
            AnalyticsSdkEventModel::ValidEventRole,
            /*value*/ QVariant::fromValue(true),
            /*hits*/ 10,
            Qt::MatchExactly | Qt::MatchRecursive);

        if (selectableItems.size())
        {
            // Use the first selectable item
            ui->sdkEventTypeComboBox->setCurrentIndex(selectableItems.front());

            engineId = ui->sdkEventTypeComboBox->currentData(
                AnalyticsSdkEventModel::EngineIdRole).value<QnUuid>();

            eventTypeId = ui->sdkEventTypeComboBox->currentData(
                AnalyticsSdkEventModel::EventTypeIdRole).value<QString>();

            model()->setEventParams(createEventParameters(engineId, eventTypeId));
        }
    }
    else
    {
        auto items = analyticsModel->match(
            analyticsModel->index(0, 0),
            AnalyticsSdkEventModel::EventTypeIdRole,
            /*value*/ QVariant::fromValue(eventTypeId),
            /*hits*/ 1,
            Qt::MatchExactly | Qt::MatchRecursive);

        if (items.size() == 1)
            ui->sdkEventTypeComboBox->setCurrentIndex(items.front());
    }
}

nx::vms::event::EventParameters AnalyticsSdkEventWidget::createEventParameters(
    const QnUuid& engineId,
    const QString& analyticsEventTypeId)
{
    auto eventParams = model()->eventParams();
    eventParams.caption = ui->captionEdit->text();
    eventParams.description = ui->descriptionEdit->text();
    eventParams.setAnalyticsEventTypeId(analyticsEventTypeId);
    eventParams.setAnalyticsEngineId(engineId);

    return eventParams;
}

} // namespace ui
} // namespace nx::vms::client::desktop
