#include "analytics_sdk_event_widget.h"
#include "ui_analytics_sdk_event_widget.h"

#include <QtCore/QSortFilterProxyModel>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <nx/vms/client/desktop/ui/event_rules/models/analytics_sdk_event_model.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

namespace nx::vms::client::desktop {
namespace ui {

AnalyticsSdkEventWidget::AnalyticsSdkEventWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::AnalyticsSdkEventWidget),
    m_sdkEventModel(new AnalyticsSdkEventModel(this))
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
    setHelpTopic(ui->sdkEventTypeLabel, Qn::EventsActions_VideoAnalytics_Help);

    ui->captionLabel->addHintLine(tr("Event will trigger only if there are matches in the caption "
        "with any of the entered keywords."));
    ui->captionLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    ui->captionLabel->addHintLine(tr("This field is case sensitive."));
    setHelpTopic(ui->captionLabel, Qn::EventsActions_VideoAnalytics_Help);

    ui->descriptionLabel->addHintLine(tr("Event will trigger only if there are matches in the "
        "description field with any of the entered keywords."));
    ui->descriptionLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    ui->descriptionLabel->addHintLine(tr("This field is case sensitive."));
    setHelpTopic(ui->descriptionLabel, Qn::EventsActions_VideoAnalytics_Help);
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
            AnalyticsSdkEventModel::DriverIdRole).value<QString>(),
        ui->sdkEventTypeComboBox->currentData(
            AnalyticsSdkEventModel::EventTypeIdRole).value<QString>()));
}

void AnalyticsSdkEventWidget::updateSdkEventTypesModel()
{
    const auto cameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
        model()->eventResources());
    m_sdkEventModel->loadFromCameras(cameras);
    ui->sdkEventTypeComboBox->setEnabled(m_sdkEventModel->isValid());
    ui->sdkEventTypeComboBox->model()->sort(0);
}

void AnalyticsSdkEventWidget::updateSelectedEventType()
{
    QString pluginId = model()->eventParams().getAnalyticsPluginId();
    QString eventTypeId = model()->eventParams().getAnalyticsEventTypeId();

    if (pluginId.isNull() || eventTypeId.isNull())
    {
        pluginId = ui->sdkEventTypeComboBox->itemData(0,
            AnalyticsSdkEventModel::DriverIdRole).value<QString>();

        eventTypeId = ui->sdkEventTypeComboBox->itemData(0,
            AnalyticsSdkEventModel::EventTypeIdRole).value<QString>();

        model()->setEventParams(createEventParameters(pluginId, eventTypeId));
    }

    auto analyticsModel = ui->sdkEventTypeComboBox->model();

    auto items = analyticsModel->match(
        analyticsModel->index(0, 0),
        AnalyticsSdkEventModel::EventTypeIdRole,
        /*value*/ qVariantFromValue(eventTypeId),
        /*hits*/ 1,
        Qt::MatchExactly | Qt::MatchRecursive);

    if (items.size() == 1)
        ui->sdkEventTypeComboBox->setCurrentIndex(items.front());
}

nx::vms::event::EventParameters AnalyticsSdkEventWidget::createEventParameters(
    const QString& pluginId,
    const QString& analyticsEventTypeId)
{
    auto eventParams = model()->eventParams();
    eventParams.caption = ui->captionEdit->text();
    eventParams.description = ui->descriptionEdit->text();
    eventParams.setAnalyticsEventTypeId(analyticsEventTypeId);
    eventParams.setAnalyticsPluginId(pluginId);

    return eventParams;
}

} // namespace ui
} // namespace nx::vms::client::desktop
