// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/common/widgets/icon_combo_box.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/event_rules/models/plugin_diagnostic_event_model.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/rules/action_builder_fields/content_type_field.h>
#include <nx/vms/rules/action_builder_fields/http_method_field.h>
#include <nx/vms/rules/event_filter_fields/analytics_engine_field.h>
#include <nx/vms/rules/event_filter_fields/customizable_icon_field.h>
#include <nx/vms/rules/event_filter_fields/input_port_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/widgets/common/tree_combo_box.h>

#include "picker_widget.h"
#include "picker_widget_strings.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

/**
 * Used for types that could be represented as a one line text and gives support by dropdown list
 * of values. Implementation is dependent on the field parameter.
 * Has implementation for:
 * - nx.actions.fields.contentType
 * - nx.actions.fields.httpMethod
 * - nx.events.fields.inputPort
 * - nx.events.fields.customizableIcon
 */
template<typename F, typename D = QComboBox>
class DropdownTextPickerWidget: public FieldPickerWidget<F>
{
public:
    explicit DropdownTextPickerWidget(common::SystemContext* context, QWidget* parent = nullptr):
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
    const QString kAutoValue = DropdownTextPickerWidgetStrings::autoValue();

    void initializeComboBox()
    {
    }

    virtual void onFieldsSet() override
    {
        customizeComboBox();

        setCurrentValue();

        connect(m_comboBox,
            &QComboBox::currentTextChanged,
            this,
            &DropdownTextPickerWidget<F, D>::onCurrentIndexChanged,
            Qt::UniqueConnection);

        connectLinkedFields();
    }

    void customizeComboBox();

    void setCurrentValue()
    {
        const auto fieldValue = m_field->value();
        {
            QSignalBlocker blocker{m_comboBox};
            if (fieldValue.isEmpty())
                m_comboBox->setCurrentText(kAutoValue);
            else
                m_comboBox->setCurrentText(fieldValue);
        }
    }

    typename F::value_type currentValue() const
    {
        return m_comboBox->currentText();
    }

    void connectLinkedFields()
    {
    }

    void onLinkedFieldChanged()
    {
        customizeComboBox();
        setCurrentValue();
        m_field->setValue(currentValue());
    }

    void onCurrentIndexChanged()
    {
        m_field->setValue(currentValue());
    }
};

using CustomizableIconPicker = DropdownTextPickerWidget<vms::rules::CustomizableIconField, IconComboBox>;

template<>
void CustomizableIconPicker::customizeComboBox()
{
    constexpr auto kDropdownIconSize = 40;
    const auto pixmapNames = SoftwareTriggerPixmaps::pixmapNames();
    const auto nextEvenValue = [](int value) { return value + (value & 1); };
    const auto columnCount = nextEvenValue(qCeil(qSqrt(pixmapNames.size())));

    auto iconComboBox = static_cast<IconComboBox*>(m_comboBox);

    iconComboBox->setColumnCount(columnCount);
    iconComboBox->setItemSize({kDropdownIconSize, kDropdownIconSize});
    iconComboBox->setPixmaps(SoftwareTriggerPixmaps::pixmapsPath(), pixmapNames);
}

template<>
void CustomizableIconPicker::setCurrentValue()
{
    QSignalBlocker blocker{m_comboBox};
    auto iconComboBox = static_cast<IconComboBox*>(m_comboBox);
    iconComboBox->setCurrentIcon(SoftwareTriggerPixmaps::effectivePixmapName(m_field->value()));
}

template<>
QString CustomizableIconPicker::currentValue() const
{
    return static_cast<IconComboBox*>(m_comboBox)->currentIcon();
}

using HttpContentTypePicker = DropdownTextPickerWidget<vms::rules::ContentTypeField>;

template<>
void HttpContentTypePicker::customizeComboBox()
{
    m_comboBox->setEditable(true);

    m_comboBox->addItem(kAutoValue);
    m_comboBox->addItem("text/plain");
    m_comboBox->addItem("text/html");
    m_comboBox->addItem("application/html");
    m_comboBox->addItem("application/json");
    m_comboBox->addItem("application/xml");
}

template<>
void HttpContentTypePicker::onCurrentIndexChanged()
{
    const auto value = currentValue().trimmed();
    if (value != kAutoValue)
        m_field->setValue(value);
    else
        m_field->setValue({});
}

using HttpMethodPicker = DropdownTextPickerWidget<vms::rules::HttpMethodField>;

template<>
void HttpMethodPicker::customizeComboBox()
{
    m_comboBox->addItem(kAutoValue);
    m_comboBox->addItem("GET");
    m_comboBox->addItem("POST");
    m_comboBox->addItem("PUT");
    m_comboBox->addItem("DELETE");
}

template<>
void HttpMethodPicker::onCurrentIndexChanged()
{
    const auto value = currentValue().trimmed();
    if (value != kAutoValue)
        m_field->setValue(value);
    else
        m_field->setValue({});
}

using InputPortPicker = DropdownTextPickerWidget<vms::rules::InputPortField>;

template<>
void InputPortPicker::setCurrentValue()
{
    const auto fieldValue = m_field->value();
    const auto valueIndex = m_comboBox->findData(fieldValue);
    {
        QSignalBlocker blocker{m_comboBox};
        m_comboBox->setCurrentIndex(valueIndex != -1 ? valueIndex : 0);
    }
}

template<>
QString InputPortPicker::currentValue() const
{
    return m_comboBox->currentData().toString();
}

template<>
void InputPortPicker::customizeComboBox()
{
    using vms::rules::SourceCameraField;

    QSignalBlocker blocker{m_comboBox};

    m_comboBox->clear();

    auto sourceCameraField = dynamic_cast<SourceCameraField*>(
        m_linkedFields.value(vms::rules::utils::kCameraIdFieldName).data());

    if (!NX_ASSERT(sourceCameraField))
        return;

    m_comboBox->addItem(kAutoValue, QString());

    QnVirtualCameraResourceList cameras;

    if (sourceCameraField->acceptAll())
        cameras = resourcePool()->getAllCameras();
    else
        cameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(sourceCameraField->ids());

    const auto inputComparator =
        [](const QnIOPortData& l, const QnIOPortData& r)
        {
            return l.id + l.getName() < r.id + r.getName();
        };

    const auto getSortedInputs =
        [&inputComparator](const QnVirtualCameraResourcePtr& camera)
        {
            auto cameraInputs = camera->ioPortDescriptions(Qn::PT_Input);
            std::sort(cameraInputs.begin(), cameraInputs.end(), inputComparator);

            return cameraInputs;
        };

    QnIOPortDataList totalInputs;
    bool isFirstIteration = true;

    // If multiple cameras chosen, only ports present in all the chosen cameras must be
    // available to be selected by the user.
    for (const auto& camera: cameras)
    {
        if (isFirstIteration)
        {
            totalInputs = getSortedInputs(camera);
            isFirstIteration = false;
            continue;
        }

        if (totalInputs.size() == 0)
            return; //< There is no sense to check other cameras, only auto value is available.

        QnIOPortDataList intersection;
        const auto cameraInputs = getSortedInputs(camera);

        std::set_intersection(
            totalInputs.cbegin(),
            totalInputs.cend(),
            cameraInputs.cbegin(),
            cameraInputs.cend(),
            std::back_inserter(intersection),
            inputComparator);

        totalInputs = std::move(intersection);
    }

    for (const auto& cameraInput: totalInputs)
        m_comboBox->addItem(cameraInput.getName(), cameraInput.id);
}

template<>
void InputPortPicker::connectLinkedFields()
{
    using vms::rules::SourceCameraField;
    auto sourceCameraField =
        getLinkedField<SourceCameraField>(vms::rules::utils::kCameraIdFieldName);
    if (!NX_ASSERT(sourceCameraField))
        return;

    connect(
        sourceCameraField,
        &SourceCameraField::acceptAllChanged,
        this,
        &InputPortPicker::onLinkedFieldChanged);

    connect(
        sourceCameraField,
        &SourceCameraField::idsChanged,
        this,
        &InputPortPicker::onLinkedFieldChanged);
}

using AnalyticsEnginePicker = DropdownTextPickerWidget<vms::rules::AnalyticsEngineField, QnTreeComboBox>;

template<>
void AnalyticsEnginePicker::initializeComboBox()
{
    auto treeComboBox = static_cast<QnTreeComboBox*>(m_comboBox);

    auto pluginDiagnosticEventModel = new ui::PluginDiagnosticEventModel(this);
    auto sortModel = new QSortFilterProxyModel(this);
    sortModel->setDynamicSortFilter(true);
    sortModel->setSourceModel(pluginDiagnosticEventModel);

    treeComboBox->setModel(sortModel);
}

template<>
void AnalyticsEnginePicker::setCurrentValue()
{
    auto treeComboBox = static_cast<QnTreeComboBox*>(m_comboBox);
    QSignalBlocker blocker{treeComboBox};

    QnUuid pluginId = m_field->value();
    if (pluginId.isNull())
    {
        pluginId = treeComboBox->itemData(
            0,
            ui::PluginDiagnosticEventModel::PluginIdRole).value<QnUuid>();
    }

    auto pluginListModel = treeComboBox->model();

    auto items = pluginListModel->match(
        pluginListModel->index(0, 0),
        ui::PluginDiagnosticEventModel::PluginIdRole,
        /*value*/ QVariant::fromValue(pluginId),
        /*hits*/ 1,
        Qt::MatchExactly | Qt::MatchRecursive);

    if (items.size() == 1)
        treeComboBox->setCurrentIndex(items.front());
}

template<>
QnUuid AnalyticsEnginePicker::currentValue() const
{
    const auto treeComboBox = static_cast<QnTreeComboBox*>(m_comboBox);
    return treeComboBox->currentData(ui::PluginDiagnosticEventModel::PluginIdRole).value<QnUuid>();
}

template<>
void AnalyticsEnginePicker::customizeComboBox()
{
    using vms::rules::SourceCameraField;

    auto treeComboBox = static_cast<QnTreeComboBox*>(m_comboBox);
    QSignalBlocker blocker{treeComboBox};

    treeComboBox->clear();

    auto sourceCameraField
        = getLinkedField<SourceCameraField>(vms::rules::utils::kCameraIdFieldName);
    if (!NX_ASSERT(sourceCameraField))
        return;

    QnVirtualCameraResourceList cameras;

    if (sourceCameraField->acceptAll())
        cameras = resourcePool()->getAllCameras();
    else
        cameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(sourceCameraField->ids());

    auto sourceModel = static_cast<ui::PluginDiagnosticEventModel*>(
        static_cast<QSortFilterProxyModel*>(treeComboBox->model())->sourceModel());

    sourceModel->filterByCameras(
        resourcePool()->getResources<vms::common::AnalyticsEngineResource>(),
        cameras);

    treeComboBox->setEnabled(sourceModel->isValid());
    treeComboBox->model()->sort(0);
}

template<>
void AnalyticsEnginePicker::connectLinkedFields()
{
    using vms::rules::SourceCameraField;

    auto sourceCameraField =
        getLinkedField<SourceCameraField>(vms::rules::utils::kCameraIdFieldName);
    if (!NX_ASSERT(sourceCameraField))
        return;

    connect(
        sourceCameraField,
        &SourceCameraField::acceptAllChanged,
        this,
        &AnalyticsEnginePicker::onLinkedFieldChanged);

    connect(
        sourceCameraField,
        &SourceCameraField::idsChanged,
        this,
        &AnalyticsEnginePicker::onLinkedFieldChanged);
}

} // namespace nx::vms::client::desktop::rules
