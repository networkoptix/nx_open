#include "analytics_sdk_event_widget.h"
#include "ui_analytics_sdk_event_widget.h"

#include <QtCore/QSortFilterProxyModel>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <nx/client/desktop/ui/event_rules/models/analytics_sdk_event_model.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/scoped_value_rollback.h>

namespace nx {
namespace client {
namespace desktop {
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

    const QString description = tr("Event will trigger only if Analytics Event meets all the above conditions. "
        "If a keyword field is empty, condition is always met. "
        "If not, condition is met if the corresponding field of Analytics Event contains any keyword.");

    ui->hintLabel->setText(description);
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
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

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
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    model()->setEventParams(createEventParameters(
        ui->sdkEventTypeComboBox->currentData(
            AnalyticsSdkEventModel::DriverIdRole).value<QnUuid>(),
        ui->sdkEventTypeComboBox->currentData(
            AnalyticsSdkEventModel::EventTypeIdRole).value<QnUuid>()));
}

void AnalyticsSdkEventWidget::updateSdkEventTypesModel()
{
    const auto cameras = resourcePool()->getResources<QnVirtualCameraResource>(
        model()->eventResources());
    m_sdkEventModel->loadFromCameras(cameras);
    ui->sdkEventTypeComboBox->setEnabled(m_sdkEventModel->isValid());
    ui->sdkEventTypeComboBox->model()->sort(0);
}

void AnalyticsSdkEventWidget::updateSelectedEventType()
{
    QnUuid driverId = model()->eventParams().analyticsDriverId();
    QnUuid eventTypeId = model()->eventParams().analyticsEventId();

    if (driverId.isNull() || eventTypeId.isNull())
    {
        driverId = ui->sdkEventTypeComboBox->itemData(0,
            AnalyticsSdkEventModel::DriverIdRole).value<QnUuid>();

        eventTypeId = ui->sdkEventTypeComboBox->itemData(0,
            AnalyticsSdkEventModel::EventTypeIdRole).value<QnUuid>();

        model()->setEventParams(createEventParameters(driverId, eventTypeId));
    }

    int index = 0;
    for (int i = 0; i < ui->sdkEventTypeComboBox->count(); ++i)
    {
        auto itemDriverId = ui->sdkEventTypeComboBox->itemData(i,
            AnalyticsSdkEventModel::DriverIdRole).value<QnUuid>();

        if (itemDriverId != driverId)
            continue;

        auto itemEventTypeId = ui->sdkEventTypeComboBox->itemData(i,
            AnalyticsSdkEventModel::EventTypeIdRole).value<QnUuid>();
        if (itemEventTypeId != eventTypeId)
            continue;

        index = i;
        break;
    }

    ui->sdkEventTypeComboBox->setCurrentIndex(index);    
}

nx::vms::event::EventParameters AnalyticsSdkEventWidget::createEventParameters(
    const QnUuid& driverId,
    const QnUuid& analyticsEventTypeId)
{
    auto eventParams = model()->eventParams();
    eventParams.caption = ui->captionEdit->text();
    eventParams.description = ui->descriptionEdit->text();
    eventParams.setAnalyticsEventId(analyticsEventTypeId);
    eventParams.setAnalyticsDriverId(driverId);

    return eventParams;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
