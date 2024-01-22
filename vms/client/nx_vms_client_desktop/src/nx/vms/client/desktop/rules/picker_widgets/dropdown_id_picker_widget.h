// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QHBoxLayout>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/event_rules/widgets/detectable_object_type_combo_box.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ui/event_rules/models/analytics_sdk_event_model.h>
#include <nx/vms/client/desktop/ui/event_rules/models/plugin_diagnostic_event_model.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/rules/event_filter_fields/analytics_engine_field.h>
#include <nx/vms/rules/event_filter_fields/analytics_event_type_field.h>
#include <nx/vms/rules/event_filter_fields/analytics_object_type_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/widgets/common/elided_label.h>
#include <ui/widgets/common/tree_combo_box.h>

#include "field_picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

template<typename F, typename D>
class DropdownIdPickerWidgetBase: public PlainFieldPickerWidget<F>
{
    using base = PlainFieldPickerWidget<F>;

public:
    DropdownIdPickerWidgetBase(SystemContext* context, ParamsWidget* parent):
        base(context, parent)
    {
        auto contentLayout = new QHBoxLayout;
        m_comboBox = new D;
        m_comboBox->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        contentLayout->addWidget(m_comboBox);

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_comboBox,
            &D::currentIndexChanged,
            this,
            &DropdownIdPickerWidgetBase<F, D>::onCurrentIndexChanged);
    }

protected:
    BASE_COMMON_USINGS

    D* m_comboBox{nullptr};

    virtual void onCurrentIndexChanged() = 0;

    QnVirtualCameraResourceList getCameras() const
    {
        using vms::rules::SourceCameraField;

        auto sourceCameraField =
            this->template getEventField<SourceCameraField>(vms::rules::utils::kCameraIdFieldName);

        if (!NX_ASSERT(sourceCameraField))
            return {};

        QnVirtualCameraResourceList cameras;

        if (sourceCameraField->acceptAll())
            return resourcePool()->getAllCameras();

        return resourcePool()->template getResourcesByIds<QnVirtualCameraResource>(
            sourceCameraField->ids());
    }
};

class AnalyticsEnginePicker:
    public DropdownIdPickerWidgetBase<vms::rules::AnalyticsEngineField, QnTreeComboBox>
{
public:
    AnalyticsEnginePicker(SystemContext* context, ParamsWidget* parent):
        DropdownIdPickerWidgetBase<vms::rules::AnalyticsEngineField, QnTreeComboBox>(context, parent)
    {
        m_pluginDiagnosticEventModel = new ui::PluginDiagnosticEventModel(this);
        auto sortModel = new QSortFilterProxyModel(this);
        sortModel->setDynamicSortFilter(true);
        sortModel->setSourceModel(m_pluginDiagnosticEventModel);

        m_comboBox->setModel(sortModel);
    }

protected:
    void onCurrentIndexChanged() override
    {
        theField()->setValue(
            m_comboBox->currentData(ui::PluginDiagnosticEventModel::PluginIdRole).value<QnUuid>());
    }

    void updateUi()
    {
        QSignalBlocker blocker{m_comboBox};

        m_pluginDiagnosticEventModel->filterByCameras(
            resourcePool()->getResources<vms::common::AnalyticsEngineResource>(),
            getCameras());

        m_comboBox->setEnabled(m_pluginDiagnosticEventModel->isValid());
        m_comboBox->model()->sort(0);

        QnUuid pluginId = theField()->value();
        if (pluginId.isNull())
        {
            pluginId = m_comboBox->itemData(
                0,
                ui::PluginDiagnosticEventModel::PluginIdRole).value<QnUuid>();
        }

        auto pluginListModel = m_comboBox->model();

        auto items = pluginListModel->match(
            pluginListModel->index(0, 0),
            ui::PluginDiagnosticEventModel::PluginIdRole,
            /*value*/ QVariant::fromValue(pluginId),
            /*hits*/ 1,
            Qt::MatchExactly | Qt::MatchRecursive);

        if (items.size() == 1)
            m_comboBox->setCurrentIndex(items.front());
    }

private:
    ui::PluginDiagnosticEventModel* m_pluginDiagnosticEventModel{nullptr};
};

class AnalyticsEventTypePicker: public DropdownIdPickerWidgetBase<vms::rules::AnalyticsEventTypeField, QnTreeComboBox>
{
    Q_OBJECT

public:
    AnalyticsEventTypePicker(SystemContext* context, ParamsWidget* parent):
        DropdownIdPickerWidgetBase<vms::rules::AnalyticsEventTypeField, QnTreeComboBox>(context, parent)
    {
        m_analyticsSdkEventModel = new ui::AnalyticsSdkEventModel(systemContext() ,this);
        auto sortModel = new QSortFilterProxyModel(this);
        sortModel->setDynamicSortFilter(true);
        sortModel->setSourceModel(m_analyticsSdkEventModel);

        m_comboBox->setModel(sortModel);
    }

protected:
    void onDescriptorSet() override
    {
        DropdownIdPickerWidgetBase<vms::rules::AnalyticsEventTypeField,
            QnTreeComboBox>::onDescriptorSet();

        m_label->addHintLine(tr("Analytics events can be set up on a certain cameras."));
        m_label->addHintLine(
            tr("Choose cameras using the button above to see the list of supported events."));
        setHelpTopic(m_label, HelpTopic::Id::EventsActions_VideoAnalytics);
    }

    void onCurrentIndexChanged() override
    {
        theField()->setEngineId(
            m_comboBox->currentData(ui::AnalyticsSdkEventModel::EngineIdRole).value<QnUuid>());
        theField()->setTypeId(
            m_comboBox->currentData(ui::AnalyticsSdkEventModel::EventTypeIdRole).value<QString>());
    }

    void updateUi()
    {
        QSignalBlocker blocker{m_comboBox};

        m_analyticsSdkEventModel->loadFromCameras(
            getCameras(),
            m_comboBox->currentData(ui::AnalyticsSdkEventModel::EngineIdRole).value<QnUuid>(),
            m_comboBox->currentData(ui::AnalyticsSdkEventModel::EventTypeIdRole).value<QString>());

        m_comboBox->setEnabled(m_analyticsSdkEventModel->isValid());

        auto engineId = theField()->engineId();
        auto typeId = theField()->typeId();

        auto analyticsModel = m_comboBox->model();

        if (engineId.isNull() || typeId.isNull())
        {
            const auto selectableItems = analyticsModel->match(
                analyticsModel->index(0, 0),
                ui::AnalyticsSdkEventModel::ValidEventRole,
                /*value*/ QVariant::fromValue(true),
                /*hits*/ 10,
                Qt::MatchExactly | Qt::MatchRecursive);

            if (!selectableItems.empty())
            {
                // Use the first selectable item
                m_comboBox->setCurrentIndex(selectableItems.front());

                // #mmalofeev Is it right to use the first available selectable item in the case when
                // engineId or typeId is not set? If do so it will lead to rule model changes which in
                // turn will leads to required apply or discard changes action from the user. Choose
                // first selectable item and do not change model is also not an option cause it may
                // leads to different values in different launches. Isn't it better to leave dropdown
                // blank?
                // updateFieldValue();
            }
        }
        else
        {
            auto items = analyticsModel->match(
                analyticsModel->index(0, 0),
                ui::AnalyticsSdkEventModel::EventTypeIdRole,
                /*value*/ QVariant::fromValue(typeId),
                /*hits*/ 1,
                Qt::MatchExactly | Qt::MatchRecursive);

            if (items.size() == 1)
                m_comboBox->setCurrentIndex(items.front());
        }
    }

private:
    ui::AnalyticsSdkEventModel* m_analyticsSdkEventModel{nullptr};
};

class AnalyticsObjectTypePicker:
    public DropdownIdPickerWidgetBase<vms::rules::AnalyticsObjectTypeField, DetectableObjectTypeComboBox>
{
    Q_OBJECT

public:
    using DropdownIdPickerWidgetBase<vms::rules::AnalyticsObjectTypeField,
        DetectableObjectTypeComboBox>::DropdownIdPickerWidgetBase;

protected:
    void onDescriptorSet() override
    {
        DropdownIdPickerWidgetBase<vms::rules::AnalyticsObjectTypeField,
            DetectableObjectTypeComboBox>::onDescriptorSet();

        m_label->addHintLine(tr("Analytics object detection can be set up on a certain cameras."));
        m_label->addHintLine(
            tr("Choose cameras using the button above to see the list of supported events."));
        setHelpTopic(m_label, HelpTopic::Id::EventsActions_VideoAnalytics);
    }

    void onCurrentIndexChanged() override
    {
        theField()->setValue(m_comboBox->selectedMainObjectTypeId());
    }

    void updateUi()
    {
        QSignalBlocker blocker{m_comboBox};

        const auto ids = getCameras().ids();
        m_comboBox->setDevices(QnUuidSet{ids.begin(), ids.end()});

        m_comboBox->setSelectedMainObjectTypeId(theField()->value());
    }
};

} // namespace nx::vms::client::desktop::rules
