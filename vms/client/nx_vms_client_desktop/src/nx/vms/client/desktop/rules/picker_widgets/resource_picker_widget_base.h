// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

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

    QPushButton* m_selectButton{nullptr};

    void updateUi() override
    {
        const auto validity = fieldValidity();
        if (validity.validity == QValidator::State::Invalid)
        {
            PlainFieldPickerWidget<F>::setValidity(fieldValidity());
            return;
        }

        // Only error state must be shown by the widget. Intermidiate state warning must be shown
        // by the resource selection dialog.
        PlainFieldPickerWidget<F>::setValidity({});
    }

    virtual void onSelectButtonClicked() = 0;
};

} // namespace nx::vms::client::desktop::rules
