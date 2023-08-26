// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin_diagnostic_event_widget.h"
#include "ui_plugin_diagnostic_event_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QSortFilterProxyModel>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/reflect/string_conversion.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ui/event_rules/models/plugin_diagnostic_event_model.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {
namespace ui {

PluginDiagnosticEventWidget::PluginDiagnosticEventWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::PluginDiagnosticEventWidget),
    m_pluginDiagnosticEventModel(new PluginDiagnosticEventModel(this))
{
    ui->setupUi(this);

    auto sortModel = new QSortFilterProxyModel(this);
    sortModel->setDynamicSortFilter(true);
    sortModel->setSourceModel(m_pluginDiagnosticEventModel);
    ui->pirComboBox->setModel(sortModel);

    connect(ui->pirComboBox, QnComboboxCurrentIndexChanged, this,
        &PluginDiagnosticEventWidget::paramsChanged);
    connect(ui->captionEdit, &QLineEdit::textChanged, this,
        &PluginDiagnosticEventWidget::paramsChanged);
    connect(ui->descriptionEdit, &QLineEdit::textChanged, this,
        &PluginDiagnosticEventWidget::paramsChanged);
    connect(ui->errorCheckBox, &QCheckBox::toggled, this,
        &PluginDiagnosticEventWidget::paramsChanged);
    connect(ui->warningCheckBox, &QCheckBox::toggled, this,
        &PluginDiagnosticEventWidget::paramsChanged);
    connect(ui->infoCheckBox, &QCheckBox::toggled, this,
        &PluginDiagnosticEventWidget::paramsChanged);

    //ui->pirLabel->addHintLine(tr("Choose plugin instance from the list"));
    //setHelpTopic(ui->pluginDiagnosticEventTypeLabel, HelpTopic::Id::EventsActions_VideoAnalytics);

    ui->captionLabel->addHintLine(tr("Event will trigger only if there are matches in the caption "
        "with any of the entered keywords."));
    ui->captionLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    ui->captionLabel->addHintLine(tr("This field is case sensitive."));
    //setHelpTopic(ui->captionLabel, HelpTopic::Id::EventsActions_VideoAnalytics);

    ui->descriptionLabel->addHintLine(tr("Event will trigger only if there are matches in the "
        "description field with any of the entered keywords."));
    ui->descriptionLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    ui->descriptionLabel->addHintLine(tr("This field is case sensitive."));
    //setHelpTopic(ui->descriptionLabel, HelpTopic::Id::EventsActions_VideoAnalytics);
}

PluginDiagnosticEventWidget::~PluginDiagnosticEventWidget() {
}

void PluginDiagnosticEventWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->pirComboBox);
    setTabOrder(ui->pirComboBox, ui->captionEdit);
    setTabOrder(ui->captionEdit, ui->descriptionEdit);
    setTabOrder(ui->descriptionEdit, ui->errorCheckBox);
    setTabOrder(ui->errorCheckBox, ui->warningCheckBox);
    setTabOrder(ui->warningCheckBox, ui->infoCheckBox);
    setTabOrder(ui->infoCheckBox, after);
}

void PluginDiagnosticEventWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::eventResources))
        updatePluginDiagnosticEventTypesModel();

    if (fields.testFlag(Field::eventParams))
    {
        QString caption = model()->eventParams().caption;
        if (ui->captionEdit->text() != caption)
            ui->captionEdit->setText(caption);

        QString description = model()->eventParams().description;
        if (ui->descriptionEdit->text() != description)
            ui->descriptionEdit->setText(description);

        using namespace nx::vms::api;
        const auto levels = model()->eventParams().getDiagnosticEventLevels();
        ui->errorCheckBox->setChecked(levels.testFlag(ErrorEventLevel));
        ui->warningCheckBox->setChecked(levels.testFlag(WarningEventLevel));
        ui->infoCheckBox->setChecked(levels.testFlag(InfoEventLevel));
    }

    if (fields.testFlag(Field::eventResources) || fields.testFlag(Field::eventParams))
        updateSelectedEventType();

    if (fields.testFlag(Field::eventResources))
    {
        // #spanasenko: We need to update metadata field.
        // (!) We must do it only after updateSelectedEventType() call.
        model()->setEventParams(createEventParameters());
    }
}

void PluginDiagnosticEventWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    model()->setEventParams(createEventParameters());
}

void PluginDiagnosticEventWidget::updatePluginDiagnosticEventTypesModel()
{
    const auto cameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
        model()->eventResources());

    m_pluginDiagnosticEventModel->filterByCameras(
        resourcePool()->getResources<nx::vms::common::AnalyticsEngineResource>(),
        cameras);

    ui->pirComboBox->setEnabled(m_pluginDiagnosticEventModel->isValid());
    ui->pirComboBox->model()->sort(0);
}

void PluginDiagnosticEventWidget::updateSelectedEventType()
{
    QnUuid pluginId = model()->eventParams().eventResourceId;

    if (pluginId.isNull())
    {
        pluginId = ui->pirComboBox->itemData(
            0,
            PluginDiagnosticEventModel::PluginIdRole).value<QnUuid>();

        model()->setEventParams(createEventParameters());
    }

    auto pluginListModel = ui->pirComboBox->model();

    auto items = pluginListModel->match(
        pluginListModel->index(0, 0),
        PluginDiagnosticEventModel::PluginIdRole,
        /*value*/ QVariant::fromValue(pluginId),
        /*hits*/ 1,
        Qt::MatchExactly | Qt::MatchRecursive);

    if (items.size() == 1)
        ui->pirComboBox->setCurrentIndex(items.front());
}

nx::vms::event::EventParameters PluginDiagnosticEventWidget::createEventParameters()
{
    // TODO: #spanasenko Split this function into resources and parameters parts?
    auto eventParams = model()->eventParams();

    eventParams.eventResourceId = ui->pirComboBox->currentData(
        PluginDiagnosticEventModel::PluginIdRole).value<QnUuid>();
    eventParams.caption = ui->captionEdit->text();
    eventParams.description = ui->descriptionEdit->text();

    // Metadata is used to check parameters in PluginDiagnosticEvent::checkEventParams().
    eventParams.metadata.cameraRefs.clear();
    for (const auto& camera: model()->eventResources())
        eventParams.metadata.cameraRefs.push_back(camera.toString());

    using namespace nx::vms::api;
    EventLevels levels;
    if (ui->errorCheckBox->isChecked())
        levels.setFlag(ErrorEventLevel);
    if (ui->warningCheckBox->isChecked())
        levels.setFlag(WarningEventLevel);
    if (ui->infoCheckBox->isChecked())
        levels.setFlag(InfoEventLevel);
    eventParams.setDiagnosticEventLevels(levels);

    return eventParams;
}

} // namespace ui
} // namespace nx::vms::client::desktop
