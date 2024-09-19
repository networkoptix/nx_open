// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/rules/action_builder_fields/text_field.h>
#include <nx/vms/rules/utils/field.h>

#include "../utils/icons.h"
#include "../utils/strings.h"
#include "field_picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

template<typename F>
class ResourcePickerWidgetBase: public PlainFieldPickerWidget<F>
{
    using base = PlainFieldPickerWidget<F>;

public:
    ResourcePickerWidgetBase(F* field, SystemContext* context, ParamsWidget* parent):
        base(field, context, parent)
    {
        auto contentLayout = new QVBoxLayout;

        m_selectButton = new QPushButton;
        m_selectButton->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        contentLayout->addWidget(m_selectButton);

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_selectButton,
            &QPushButton::clicked,
            this,
            &ResourcePickerWidgetBase<F>::onSelectButtonClicked);
    }

protected:
    BASE_COMMON_USINGS
    using PlainFieldPickerWidget<F>::fieldValidity;
    using PlainFieldPickerWidget<F>::systemContext;
    using PlainFieldPickerWidget<F>::getActionField;
    using PlainFieldPickerWidget<F>::isEdited;

    QPushButton* m_selectButton{nullptr};

    void updateUi() override
    {
        PlainFieldPickerWidget<F>::updateUi();

        updateSelectButtonUi();
    }

    void setValidity(const vms::rules::ValidationResult& validationResult) override
    {
        if (validationResult.validity == QValidator::State::Invalid)
        {
            setErrorStyle(m_selectButton);
            PlainFieldPickerWidget<F>::setValidity(validationResult);
        }
        else
        {
            // Only error state must be shown by the widget. Intermidiate state warning must be shown
            // by the resource selection dialog.
            resetErrorStyle(m_selectButton);
            PlainFieldPickerWidget<F>::setValidity({});
        }
    }

    void updateSelectButtonUi();

    virtual void onSelectButtonClicked()
    {
        setEdited();
    }
};

template<class F>
void ResourcePickerWidgetBase<F>::updateSelectButtonUi()
{
    m_selectButton->setText(Strings::selectButtonText(systemContext(), m_field));
    m_selectButton->setIcon(
        selectButtonIcon(systemContext(), m_field, fieldValidity().validity));
}

template<>
inline void ResourcePickerWidgetBase<nx::vms::rules::TargetUsersField>::updateSelectButtonUi()
{
    int additionalCount{0};
    if (const auto additionalRecipients =
        this->template getActionField<vms::rules::ActionTextField>(vms::rules::utils::kEmailsFieldName))
    {
        if (!additionalRecipients->value().isEmpty())
        {
            const auto emails = additionalRecipients->value().split(';', Qt::SkipEmptyParts);
            additionalCount = static_cast<int>(emails.size());
        }
    }

    m_selectButton->setText(
        Strings::selectButtonText(systemContext(), m_field, additionalCount));
    m_selectButton->setIcon(
        selectButtonIcon(systemContext(), m_field, additionalCount, fieldValidity().validity));
}

} // namespace nx::vms::client::desktop::rules
