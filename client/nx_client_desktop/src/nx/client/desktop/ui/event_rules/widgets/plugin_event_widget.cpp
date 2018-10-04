#include "plugin_event_widget.h"
#include "ui_plugin_event_widget.h"

#include <QtCore/QSortFilterProxyModel>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <nx/client/desktop/ui/event_rules/models/plugin_event_model.h>
#include <nx/vms/common/resource/metadata_plugin_instance_resource.h>

#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

PluginEventWidget::PluginEventWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::PluginEventWidget),
    m_pluginEventModel(new PluginEventModel(this))
{
    ui->setupUi(this);

    auto sortModel = new QSortFilterProxyModel(this);
    sortModel->setDynamicSortFilter(true);
    sortModel->setSourceModel(m_pluginEventModel);
    ui->pirComboBox->setModel(sortModel);

    connect(ui->pirComboBox, QnComboboxCurrentIndexChanged, this,
        &PluginEventWidget::paramsChanged);
    connect(ui->captionEdit, &QLineEdit::textChanged, this,
        &PluginEventWidget::paramsChanged);
    connect(ui->descriptionEdit, &QLineEdit::textChanged, this,
        &PluginEventWidget::paramsChanged);
    connect(ui->cbError, &QCheckBox::toggled, this,
        &PluginEventWidget::paramsChanged);
    connect(ui->cbWarning, &QCheckBox::toggled, this,
        &PluginEventWidget::paramsChanged);
    connect(ui->cbInfo, &QCheckBox::toggled, this,
        &PluginEventWidget::paramsChanged);

    //ui->pirLabel->addHintLine(tr("Choose plugin instance from the list"));
    //setHelpTopic(ui->pluginEventTypeLabel, Qn::EventsActions_VideoAnalytics_Help);

    ui->captionLabel->addHintLine(tr("Event will trigger only if there are matches in the caption "
        "with any of the entered keywords."));
    ui->captionLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    ui->captionLabel->addHintLine(tr("This field is case sensitive."));
    //setHelpTopic(ui->captionLabel, Qn::EventsActions_VideoAnalytics_Help);

    ui->descriptionLabel->addHintLine(tr("Event will trigger only if there are matches in the "
        "description field with any of the entered keywords."));
    ui->descriptionLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    ui->descriptionLabel->addHintLine(tr("This field is case sensitive."));
    //setHelpTopic(ui->descriptionLabel, Qn::EventsActions_VideoAnalytics_Help);
}

PluginEventWidget::~PluginEventWidget() {
}

void PluginEventWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->captionEdit);
    setTabOrder(ui->captionEdit, ui->descriptionEdit);
    setTabOrder(ui->descriptionEdit, after);
}

void PluginEventWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;
    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::eventResources))
        updatePluginEventTypesModel();

    if (fields.testFlag(Field::eventParams))
    {
        QString caption = model()->eventParams().caption;
        if (ui->captionEdit->text() != caption)
            ui->captionEdit->setText(caption);

        QString description = model()->eventParams().description;
        if (ui->descriptionEdit->text() != description)
            ui->descriptionEdit->setText(description);

        QString levels = model()->eventParams().inputPortId;
        ui->cbError->setChecked(levels.contains('E'));
        ui->cbWarning->setChecked(levels.contains('W'));
        ui->cbInfo->setChecked(levels.contains('I'));
    }

    if (fields.testFlag(Field::eventResources) || fields.testFlag(Field::eventParams))
        updateSelectedEventType();
}

void PluginEventWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;
    QScopedValueRollback<bool> guard(m_updating, true);

    model()->setEventParams(createEventParameters());
}

void PluginEventWidget::updatePluginEventTypesModel()
{
    const auto pirs = resourcePool()->getResources<nx::vms::common::MetadataPluginInstanceResource>();
    m_pluginEventModel->buildFromList(pirs);
    ui->pirComboBox->setEnabled(m_pluginEventModel->isValid());
    ui->pirComboBox->model()->sort(0);
}

void PluginEventWidget::updateSelectedEventType()
{
    QnUuid pluginId = model()->eventParams().eventResourceId;

    if (pluginId.isNull())
    {
        pluginId = ui->pirComboBox->itemData(0, PluginEventModel::PluginIdRole).value<QnUuid>();

        model()->setEventParams(createEventParameters());
    }

    auto pluginListModel = ui->pirComboBox->model();

    auto items = pluginListModel->match(
        pluginListModel->index(0, 0),
        PluginEventModel::PluginIdRole,
        /*value*/ qVariantFromValue(pluginId),
        /*hits*/ 1,
        Qt::MatchExactly | Qt::MatchRecursive);

    if (items.size() == 1)
        ui->pirComboBox->setCurrentIndex(items.front());
}

nx::vms::event::EventParameters PluginEventWidget::createEventParameters()
{
    auto eventParams = model()->eventParams();

    eventParams.eventResourceId = ui->pirComboBox->currentData(PluginEventModel::PluginIdRole).value<QnUuid>();
    eventParams.caption = ui->captionEdit->text();
    eventParams.description = ui->descriptionEdit->text();

    QString& levels = eventParams.inputPortId;
    levels.clear();
    if (ui->cbError->isChecked()) levels += "E";
    if (ui->cbWarning->isChecked()) levels += "W";
    if (ui->cbInfo->isChecked()) levels += "I";

    return eventParams;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
