// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/widgets/common/elided_label.h>

#include "picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

template<typename F, typename B>
class ResourcePickerWidgetBase: public SimpleFieldPickerWidget<F>
{
public:
    explicit ResourcePickerWidgetBase(SystemContext* context, CommonParamsWidget* parent):
        SimpleFieldPickerWidget<F>(context, parent)
    {
        auto contentLayout = new QVBoxLayout;
        contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        m_selectResourceWidget = new QWidget;
        auto selectResourceLayout = new QHBoxLayout;

        m_selectButton = new B;
        m_selectButton->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        selectResourceLayout->addWidget(m_selectButton);

        {
            auto alertWidget = new QWidget;
            alertWidget->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));

            auto alertLayout = new QHBoxLayout;
            alertLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

            m_alertLabel = new QnElidedLabel;
            setWarningStyle(m_alertLabel);
            m_alertLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_alertLabel->setElideMode(Qt::ElideRight);
            alertLayout->addWidget(m_alertLabel);

            alertWidget->setLayout(alertLayout);

            selectResourceLayout->addWidget(alertWidget);
        }

        selectResourceLayout->setStretch(0, 5);
        selectResourceLayout->setStretch(1, 3);

        m_selectResourceWidget->setLayout(selectResourceLayout);

        contentLayout->addWidget(m_selectResourceWidget);

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_selectButton,
            &B::clicked,
            this,
            &ResourcePickerWidgetBase<F, B>::onSelectButtonClicked);
    }

protected:
    PICKER_WIDGET_COMMON_USINGS

    QWidget* m_selectResourceWidget{nullptr};
    B* m_selectButton{nullptr};
    QnElidedLabel* m_alertLabel{nullptr};

    virtual void onSelectButtonClicked() = 0;
};

} // namespace nx::vms::client::desktop::rules
