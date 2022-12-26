// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QHBoxLayout>

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/event_rules/widgets/detectable_object_type_combo_box.h>
#include <nx/vms/client/desktop/ui/event_rules/models/analytics_sdk_event_model.h>
#include <nx/vms/client/desktop/ui/event_rules/models/plugin_diagnostic_event_model.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/rules/event_filter_fields/analytics_engine_field.h>
#include <nx/vms/rules/event_filter_fields/analytics_event_type_field.h>
#include <nx/vms/rules/event_filter_fields/analytics_object_type_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/widgets/common/tree_combo_box.h>

#include "picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

/**
 * Used for types that could be represented as an id of some object and gives support by dropdown
 * list of values. Implementation is dependent on the field parameter.
 * Has implementation for:
 * - nx.events.fields.analyticsEngine
 * - nx.events.fields.analyticsEventType
 */
template<typename F, typename D>
class DropdownIdPickerWidget: public FieldPickerWidget<F>
{
public:
    explicit DropdownIdPickerWidget(SystemContext* context, QWidget* parent = nullptr):
        FieldPickerWidget<F>(context, parent)
    {
        auto contentLayout = new QHBoxLayout;
        m_comboBox = new D;
        m_comboBox->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        initializeComboBox();
        contentLayout->addWidget(m_comboBox);

        m_contentWidget->setLayout(contentLayout);
    }

private:
    PICKER_WIDGET_COMMON_USINGS

    D* m_comboBox{};

    void initializeComboBox()
    {
    }

    void onFieldsSet() override
    {
        customizeComboBox();
        setCurrentValue();

        connect(m_comboBox,
            &D::currentIndexChanged,
            this,
            &DropdownIdPickerWidget<F, D>::updateFieldValue,
            Qt::UniqueConnection);

        connectLinkedFields();
    }

    void customizeComboBox();

    void setCurrentValue();

    void connectLinkedFields()
    {
        using vms::rules::SourceCameraField;

        auto sourceCameraField =
            this->template getLinkedField<SourceCameraField>(vms::rules::utils::kCameraIdFieldName);

        if (!NX_ASSERT(sourceCameraField))
            return;

        connect(
            sourceCameraField,
            &SourceCameraField::acceptAllChanged,
            this,
            &DropdownIdPickerWidget<F, D>::onLinkedFieldChanged);

        connect(
            sourceCameraField,
            &SourceCameraField::idsChanged,
            this,
            &DropdownIdPickerWidget<F, D>::onLinkedFieldChanged);
    }

    void onLinkedFieldChanged()
    {
        customizeComboBox();
        setCurrentValue();
        updateFieldValue();
    }

    void updateFieldValue();

    QnVirtualCameraResourceList getCameras() const
    {
        using vms::rules::SourceCameraField;

        auto sourceCameraField
            = this->template getLinkedField<SourceCameraField>(vms::rules::utils::kCameraIdFieldName);

        if (!NX_ASSERT(sourceCameraField))
            return {};

        QnVirtualCameraResourceList cameras;

        if (sourceCameraField->acceptAll())
            return resourcePool()->getAllCameras();

        return resourcePool()->template getResourcesByIds<QnVirtualCameraResource>(sourceCameraField->ids());
    }
};

using AnalyticsEnginePicker = DropdownIdPickerWidget<vms::rules::AnalyticsEngineField, QnTreeComboBox>;

template<>
void AnalyticsEnginePicker::initializeComboBox()
{
    auto pluginDiagnosticEventModel = new ui::PluginDiagnosticEventModel(this);
    auto sortModel = new QSortFilterProxyModel(this);
    sortModel->setDynamicSortFilter(true);
    sortModel->setSourceModel(pluginDiagnosticEventModel);

    m_comboBox->setModel(sortModel);
}

template<>
void AnalyticsEnginePicker::setCurrentValue()
{
    QSignalBlocker blocker{m_comboBox};

    QnUuid pluginId = m_field->value();
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

template<>
void AnalyticsEnginePicker::updateFieldValue()
{
    m_field->setValue(m_comboBox->currentData(ui::PluginDiagnosticEventModel::PluginIdRole).value<QnUuid>());
}

template<>
void AnalyticsEnginePicker::customizeComboBox()
{
    QSignalBlocker blocker{m_comboBox};

    auto sourceModel = static_cast<ui::PluginDiagnosticEventModel*>(
        static_cast<QSortFilterProxyModel*>(m_comboBox->model())->sourceModel());

    sourceModel->filterByCameras(
        resourcePool()->getResources<vms::common::AnalyticsEngineResource>(),
        getCameras());

    m_comboBox->setEnabled(sourceModel->isValid());
    m_comboBox->model()->sort(0);
}

using AnalyticsEventTypePicker = DropdownIdPickerWidget<vms::rules::AnalyticsEventTypeField, QnTreeComboBox>;

template<>
void AnalyticsEventTypePicker::initializeComboBox()
{
    auto analyticsSdkEventModel = new ui::AnalyticsSdkEventModel(systemContext() ,this);
    auto sortModel = new QSortFilterProxyModel(this);
    sortModel->setDynamicSortFilter(true);
    sortModel->setSourceModel(analyticsSdkEventModel);

    m_comboBox->setModel(sortModel);
}

template<>
void AnalyticsEventTypePicker::setCurrentValue()
{
    QSignalBlocker blocker{m_comboBox};

    auto engineId = m_field->engineId();
    auto typeId = m_field->typeId();

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

template<>
void AnalyticsEventTypePicker::updateFieldValue()
{
    m_field->setEngineId(m_comboBox->currentData(ui::AnalyticsSdkEventModel::EngineIdRole).value<QnUuid>());
    m_field->setTypeId(m_comboBox->currentData(ui::AnalyticsSdkEventModel::EventTypeIdRole).value<QString>());
}

template<>
void AnalyticsEventTypePicker::customizeComboBox()
{
    QSignalBlocker blocker{m_comboBox};

    auto sourceModel = static_cast<ui::AnalyticsSdkEventModel*>(
        static_cast<QSortFilterProxyModel*>(m_comboBox->model())->sourceModel());
    sourceModel->loadFromCameras(
        getCameras(),
        m_comboBox->currentData(ui::AnalyticsSdkEventModel::EngineIdRole).value<QnUuid>(),
        m_comboBox->currentData(ui::AnalyticsSdkEventModel::EventTypeIdRole).value<QString>());

    m_comboBox->setEnabled(sourceModel->isValid());
}

using AnalyticsObjectTypePicker = DropdownIdPickerWidget<vms::rules::AnalyticsObjectTypeField, DetectableObjectTypeComboBox>;

template<>
void AnalyticsObjectTypePicker::setCurrentValue()
{
    QSignalBlocker blocker{m_comboBox};

    m_comboBox->setSelectedMainObjectTypeId(m_field->value());
}

template<>
void AnalyticsObjectTypePicker::updateFieldValue()
{
    m_field->setValue(m_comboBox->selectedMainObjectTypeId());
}

template<>
void AnalyticsObjectTypePicker::customizeComboBox()
{
    QSignalBlocker blocker{m_comboBox};

    const auto ids = getCameras().ids();
    m_comboBox->setDevices(QnUuidSet{ids.begin(), ids.end()});
}

} // namespace nx::vms::client::desktop::rules
