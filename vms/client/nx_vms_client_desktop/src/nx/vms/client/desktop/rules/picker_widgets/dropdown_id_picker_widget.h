// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QHBoxLayout>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/event_rules/widgets/detectable_object_type_combo_box.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/ui/event_rules/models/analytics_sdk_event_model.h>
#include <nx/vms/client/desktop/ui/event_rules/models/plugin_diagnostic_event_model.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/rules/event_filter_fields/analytics_engine_field.h>
#include <nx/vms/rules/event_filter_fields/analytics_event_type_field.h>
#include <nx/vms/rules/event_filter_fields/analytics_object_type_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/widgets/common/tree_combo_box.h>

#include "../utils/strings.h"
#include "field_picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

template<typename F, typename D>
class DropdownIdPickerWidgetBase: public PlainFieldPickerWidget<F>
{
    using base = PlainFieldPickerWidget<F>;

public:
    DropdownIdPickerWidgetBase(F* field, SystemContext* context, ParamsWidget* parent):
        base(field, context, parent)
    {
        auto contentLayout = new QHBoxLayout;

        m_comboBox = new D;
        m_comboBox->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        contentLayout->addWidget(m_comboBox);

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_comboBox,
            &D::activated,
            this,
            &DropdownIdPickerWidgetBase<F, D>::onActivated);
    }

    void setValidity(const vms::rules::ValidationResult& validationResult) override
    {
        base::setValidity(validationResult);

        if (validationResult.validity == QValidator::State::Invalid)
            setErrorStyle(m_comboBox);
        else
            resetErrorStyle(m_comboBox);
    }

protected:
    BASE_COMMON_USINGS

    D* m_comboBox{nullptr};

    virtual void onActivated()
    {
        setEdited();
    }

    QnVirtualCameraResourceList getCameras() const
    {
        using vms::rules::SourceCameraField;

        auto sourceCameraField =
            this->template getEventField<SourceCameraField>(vms::rules::utils::kCameraIdFieldName);

        if (!NX_ASSERT(sourceCameraField))
            return {};

        return sourceCameraField->acceptAll()
            ? QnVirtualCameraResourceList{}
            : resourcePool()->template getResourcesByIds<QnVirtualCameraResource>(
                sourceCameraField->ids());
    }
};

class AnalyticsEnginePicker:
    public DropdownIdPickerWidgetBase<vms::rules::AnalyticsEngineField, QnTreeComboBox>
{
public:
    AnalyticsEnginePicker(
        vms::rules::AnalyticsEngineField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        DropdownIdPickerWidgetBase<vms::rules::AnalyticsEngineField, QnTreeComboBox>(
            field, context, parent),
        m_pluginDiagnosticEventModel{new ui::PluginDiagnosticEventModel(this)}
    {
        m_comboBox->setModel(m_pluginDiagnosticEventModel);
    }

protected:
    void onActivated() override
    {
        m_field->setValue(
            m_comboBox->currentData(ui::PluginDiagnosticEventModel::PluginIdRole).value<nx::Uuid>());

        DropdownIdPickerWidgetBase<vms::rules::AnalyticsEngineField, QnTreeComboBox>::onActivated();
    }

    void updateUi() override
    {
        DropdownIdPickerWidgetBase<vms::rules::AnalyticsEngineField, QnTreeComboBox>::updateUi();

        m_pluginDiagnosticEventModel->filterByCameras(
            resourcePool()->getResources<vms::common::AnalyticsEngineResource>(),
            getCameras());

        m_comboBox->setEnabled(m_pluginDiagnosticEventModel->isValid());

        nx::Uuid pluginId = m_field->value();
        if (pluginId.isNull())
        {
            pluginId = m_comboBox->itemData(
                0,
                ui::PluginDiagnosticEventModel::PluginIdRole).value<nx::Uuid>();
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
    AnalyticsEventTypePicker(
        vms::rules::AnalyticsEventTypeField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        DropdownIdPickerWidgetBase<vms::rules::AnalyticsEventTypeField, QnTreeComboBox>(
            field, context, parent),
        m_analyticsSdkEventModel(new ui::AnalyticsSdkEventModel(systemContext(), this))
    {
        m_label->addHintLine(tr("Analytics events can be set up on a certain cameras."));
        m_label->addHintLine(
            tr("Choose cameras using the button above to see the list of supported events."));
        setHelpTopic(m_label, HelpTopic::Id::EventsActions_VideoAnalytics);

        auto sortModel = new QSortFilterProxyModel(this);
        sortModel->setDynamicSortFilter(true);
        sortModel->setSourceModel(m_analyticsSdkEventModel);

        m_comboBox->setObjectName("analyticsEventTypeComboBox");
        m_comboBox->setModel(sortModel);

        const auto sourceCameraField =
            getEventField<vms::rules::SourceCameraField>(vms::rules::utils::kCameraIdFieldName);
        if (NX_ASSERT(sourceCameraField))
        {
            connect(
                sourceCameraField,
                &vms::rules::SourceCameraField::idsChanged,
                this,
                &AnalyticsEventTypePicker::onSelectedCamerasChanged);
        }
    }

protected:
    void onActivated() override
    {
        m_engineId =
            m_comboBox->currentData(ui::AnalyticsSdkEventModel::EngineIdRole).value<nx::Uuid>();
        m_field->setTypeId(
            m_comboBox->currentData(ui::AnalyticsSdkEventModel::EventTypeIdRole).value<QString>());

        DropdownIdPickerWidgetBase<vms::rules::AnalyticsEventTypeField, QnTreeComboBox>::onActivated();
    }

    void updateUi() override
    {
        DropdownIdPickerWidgetBase<vms::rules::AnalyticsEventTypeField, QnTreeComboBox>::updateUi();

        m_analyticsSdkEventModel->loadFromCameras(
            getCameras(), m_engineId, m_field->typeId());

        const auto analyticsModel = m_comboBox->model();
        auto items = analyticsModel->match(
            analyticsModel->index(0, 0),
            ui::AnalyticsSdkEventModel::EventTypeIdRole,
            /*value*/ QVariant::fromValue(m_field->typeId()),
            /*hits*/ 1,
            Qt::MatchExactly | Qt::MatchRecursive);

        m_comboBox->setEnabled(m_analyticsSdkEventModel->isValid());
        m_comboBox->setCurrentIndex(items.size() == 1 ? items.front() : QModelIndex{});
    }

private:
    ui::AnalyticsSdkEventModel* m_analyticsSdkEventModel{nullptr};
    nx::Uuid m_engineId;

    void onSelectedCamerasChanged()
    {
        m_analyticsSdkEventModel->loadFromCameras(getCameras(), {}, {});

        if (!m_analyticsSdkEventModel->isValid())
        {
            m_field->setTypeId({});
            m_engineId = {};
            return;
        }

        if (m_engineId.isNull() || m_field->typeId().isNull())
        {
            const auto analyticsModel = m_comboBox->model();
            const auto selectableItems = analyticsModel->match(
                analyticsModel->index(0, 0),
                ui::AnalyticsSdkEventModel::ValidEventRole,
                /*value*/ QVariant::fromValue(true),
                /*hits*/ 10,
                Qt::MatchExactly | Qt::MatchRecursive);

            if (!selectableItems.empty())
            {
                const auto firstItem = selectableItems.first();
                m_field->setTypeId(
                    firstItem.data(ui::AnalyticsSdkEventModel::EventTypeIdRole).toString());
                m_engineId =
                    firstItem.data(ui::AnalyticsSdkEventModel::EngineIdRole).value<nx::Uuid>();
            }
        }
    }
};

class AnalyticsObjectTypePicker:
    public DropdownIdPickerWidgetBase<vms::rules::AnalyticsObjectTypeField, DetectableObjectTypeComboBox>
{
    Q_OBJECT

public:
    using DropdownIdPickerWidgetBase<vms::rules::AnalyticsObjectTypeField,
        DetectableObjectTypeComboBox>::DropdownIdPickerWidgetBase;

    AnalyticsObjectTypePicker(
        vms::rules::AnalyticsObjectTypeField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        DropdownIdPickerWidgetBase<vms::rules::AnalyticsObjectTypeField, DetectableObjectTypeComboBox>{field, context, parent}
    {
        m_label->addHintLine(tr("Analytics object detection can be set up on a certain cameras."));
        m_label->addHintLine(
            tr("Choose cameras using the button above to see the list of supported events."));
        setHelpTopic(m_label, HelpTopic::Id::EventsActions_VideoAnalytics);
    }

protected:
    void onActivated() override
    {
        m_field->setValue(m_comboBox->selectedMainObjectTypeId());

        DropdownIdPickerWidgetBase<vms::rules::AnalyticsObjectTypeField, DetectableObjectTypeComboBox>::onActivated();
    }

    void updateUi() override
    {
        DropdownIdPickerWidgetBase<vms::rules::AnalyticsObjectTypeField, DetectableObjectTypeComboBox>::updateUi();

        const auto ids = getCameras().ids();
        m_comboBox->setDevices(UuidSet{ids.begin(), ids.end()});

        m_comboBox->setSelectedMainObjectTypeId(m_field->value());
        m_comboBox->setPlaceholderText(m_comboBox->count() == 0 ? "" : Strings::selectString());
    }
};

} // namespace nx::vms::client::desktop::rules
