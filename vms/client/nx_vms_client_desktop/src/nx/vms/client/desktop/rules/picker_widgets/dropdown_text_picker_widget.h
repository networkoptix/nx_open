// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/action_builder_fields/content_type_field.h>
#include <nx/vms/rules/action_builder_fields/http_method_field.h>
#include <nx/vms/rules/event_filter_fields/input_port_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/widgets/common/elided_label.h>

#include "picker_widget.h"
#include "picker_widget_strings.h"

namespace nx::vms::client::desktop::rules {

/**
 * Used for types that could be represented as a one line text and gives support by dropdown list
 * of values. Implementation is dependent on the field parameter.
 * Has implementation for:
 * - nx.actions.fields.contentType
 * - nx.actions.fields.httpMethod
 * - nx.events.fields.inputPort
 */
template<typename F>
class DropdownTextPickerWidget: public FieldPickerWidget<F>
{
public:
    explicit DropdownTextPickerWidget(common::SystemContext* context, QWidget* parent = nullptr):
        FieldPickerWidget<F>(context, parent)
    {
        auto mainLayout = new QHBoxLayout;
        mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        label = new QnElidedLabel;
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        mainLayout->addWidget(label);

        comboBox = new QComboBox;
        comboBox->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        mainLayout->addWidget(comboBox);

        mainLayout->setStretch(0, 1);
        mainLayout->setStretch(1, 5);

        setLayout(mainLayout);
    }

    virtual void setReadOnly(bool value) override
    {
        comboBox->setEnabled(!value);
    }

private:
    using FieldPickerWidget<F>::connect;
    using FieldPickerWidget<F>::setLayout;
    using FieldPickerWidget<F>::tr;
    using FieldPickerWidget<F>::fieldDescriptor;
    using FieldPickerWidget<F>::field;

    QnElidedLabel* label{};
    QComboBox* comboBox{};
    const QString kAutoValue = DropdownTextPickerWidgetStrings::autoValue();

    virtual void onDescriptorSet() override
    {
        label->setText(fieldDescriptor->displayName);
    }

    virtual void onFieldsSet() override
    {

        customizeComboBox();

        setCurrentText();

        connect(comboBox,
            &QComboBox::currentTextChanged,
            this,
            &DropdownTextPickerWidget<F>::onCurrentTextChanged,
            Qt::UniqueConnection);

        connectLinkedFields();
    }

    void customizeComboBox();

    void setCurrentText()
    {
        const auto fieldValue = field->value();
        {
            QSignalBlocker blocker{comboBox};
            if (fieldValue.isEmpty())
                comboBox->setCurrentText(kAutoValue);
            else
                comboBox->setCurrentText(fieldValue);
        }
    }

    void connectLinkedFields()
    {
    }

    void onCurrentTextChanged(const QString& text)
    {
        if (auto value = text.trimmed(); value != kAutoValue)
            field->setValue(value);
        else
            field->setValue({});
    }
};

using HttpContentTypePicker = DropdownTextPickerWidget<vms::rules::ContentTypeField>;
using HttpMethodPicker = DropdownTextPickerWidget<vms::rules::HttpMethodField>;
using InputPortPicker = DropdownTextPickerWidget<vms::rules::InputPortField>;

template<>
void HttpContentTypePicker::customizeComboBox()
{
    comboBox->setEditable(true);

    comboBox->addItem(kAutoValue);
    comboBox->addItem("text/plain");
    comboBox->addItem("text/html");
    comboBox->addItem("application/html");
    comboBox->addItem("application/json");
    comboBox->addItem("application/xml");
}

template<>
void HttpMethodPicker::customizeComboBox()
{
    comboBox->addItem(kAutoValue);
    comboBox->addItem("GET");
    comboBox->addItem("POST");
    comboBox->addItem("PUT");
    comboBox->addItem("DELETE");
}

template<>
void InputPortPicker::setCurrentText()
{
    const auto fieldValue = field->value();
    const auto valueIndex = comboBox->findData(fieldValue);
    {
        QSignalBlocker blocker{comboBox};
        comboBox->setCurrentIndex(valueIndex != -1 ? valueIndex : 0);
    }
}

template<>
void InputPortPicker::customizeComboBox()
{
    using vms::rules::SourceCameraField;

    QSignalBlocker blocker{comboBox};

    comboBox->clear();

    auto sourceCameraField = dynamic_cast<SourceCameraField*>(
        linkedFields.value(vms::rules::utils::kCameraIdFieldName).data());

    if (!NX_ASSERT(sourceCameraField))
        return;

    comboBox->addItem(kAutoValue, QString());

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
        comboBox->addItem(cameraInput.getName(), cameraInput.id);
}

template<>
void InputPortPicker::connectLinkedFields()
{
    // It is required to update available list of ports if a set of selected cameras changed.
    const auto onSourceCameraChanged =
        [this]
        {
            customizeComboBox();
            setCurrentText();
            field->setValue(comboBox->currentData().toString());
        };

    using vms::rules::SourceCameraField;
    auto sourceCameraField = dynamic_cast<SourceCameraField*>(
        linkedFields.value(vms::rules::utils::kCameraIdFieldName).data());

    if (!NX_ASSERT(sourceCameraField))
        return;

    connect(
        sourceCameraField,
        &SourceCameraField::acceptAllChanged,
        this,
        onSourceCameraChanged);

    connect(
        sourceCameraField,
        &SourceCameraField::idsChanged,
        this,
        onSourceCameraChanged);
}

template<>
void InputPortPicker::onCurrentTextChanged(const QString& /*text*/)
{
    field->setValue(comboBox->currentData().toString());
}

} // namespace nx::vms::client::desktop::rules
