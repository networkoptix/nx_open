// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/rules/action_fields/content_type_field.h>
#include <nx/vms/rules/action_fields/http_method_field.h>
#include <ui/widgets/common/elided_label.h>

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

/**
 * Used for types that could be represended as a one line text and gives support by dropdown list
 * of values. Implementation is dependent on the field parameter.
 * Has implementation for:
 * - nx.actions.fields.contentType
 * - nx.actions.fields.httpMethod
 */
template<typename F>
class DropdownTextPickerWidget: public FieldPickerWidget<F>
{
public:
    explicit DropdownTextPickerWidget(QWidget* parent = nullptr):
        FieldPickerWidget<F>(parent)
    {
        auto mainLayout = new QHBoxLayout;
        mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        label = new QnElidedLabel;
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        mainLayout->addWidget(label);

        comboBox = createComboBox();
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
    using FieldPickerWidget<F>::edited;
    using FieldPickerWidget<F>::fieldDescriptor;
    using FieldPickerWidget<F>::field;

    QnElidedLabel* label{};
    QComboBox* comboBox{};
    const QString kAutoValue = tr("Auto");

    virtual void onDescriptorSet() override
    {
        label->setText(fieldDescriptor->displayName);
    }

    virtual void onFieldSet() override
    {
        const auto currentContentType = field->value();
        {
            QSignalBlocker blocker{comboBox};
            if (currentContentType.isEmpty())
                comboBox->setCurrentText(kAutoValue);
            else
                comboBox->setCurrentText(currentContentType);
        }

        connect(comboBox,
            &QComboBox::currentTextChanged,
            this,
            &DropdownTextPickerWidget<F>::onCurrentTextChanged,
            Qt::UniqueConnection);
    }

    void onCurrentTextChanged(const QString& text)
    {
        if (auto contentType = text.trimmed(); contentType != kAutoValue)
            field->setValue(contentType);
        else
            field->setValue("");

        emit edited();
    }

    QComboBox* createComboBox();
};

using HttpContentTypePicker = DropdownTextPickerWidget<vms::rules::ContentTypeField>;
using HttpMethodPicker = DropdownTextPickerWidget<vms::rules::HttpMethodField>;

template<>
QComboBox* HttpContentTypePicker::createComboBox()
{
    auto contentTypeComboBox = new QComboBox;

    contentTypeComboBox->setEditable(true);

    contentTypeComboBox->addItem(kAutoValue);
    contentTypeComboBox->addItem("text/plain");
    contentTypeComboBox->addItem("text/html");
    contentTypeComboBox->addItem("application/html");
    contentTypeComboBox->addItem("application/json");
    contentTypeComboBox->addItem("application/xml");

    return contentTypeComboBox;
}

template<>
QComboBox* HttpMethodPicker::createComboBox()
{
    auto methodComboBox = new QComboBox;

    methodComboBox->addItem(kAutoValue);
    methodComboBox->addItem("GET");
    methodComboBox->addItem("POST");
    methodComboBox->addItem("PUT");
    methodComboBox->addItem("DELETE");

    return methodComboBox;
}

} // namespace nx::vms::client::desktop::rules
