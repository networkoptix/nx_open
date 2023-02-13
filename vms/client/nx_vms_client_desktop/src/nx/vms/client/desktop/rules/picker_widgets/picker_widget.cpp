// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/rules/params_widgets/common_params_widget.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/widgets/common/elided_label.h>

namespace nx::vms::client::desktop::rules {

using Field = vms::rules::Field;
using FieldDescriptor = vms::rules::FieldDescriptor;

PickerWidget::PickerWidget(QnWorkbenchContext* context, CommonParamsWidget* parent):
    QWidget(parent),
    QnWorkbenchContextAware(context)
{
    auto mainLayout = new QHBoxLayout;
    mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

    m_label = new WidgetWithHint<QnElidedLabel>;
    m_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_label->setElideMode(Qt::ElideRight);
    m_label->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
    mainLayout->addWidget(m_label);

    m_contentWidget = new QWidget;
    mainLayout->addWidget(m_contentWidget);

    mainLayout->setStretch(0, 1);
    mainLayout->setStretch(1, 5);

    setLayout(mainLayout);
}

void PickerWidget::setDescriptor(const FieldDescriptor& descriptor)
{
    m_fieldDescriptor = descriptor;

    onDescriptorSet();
}

std::optional<FieldDescriptor> PickerWidget::descriptor() const
{
    return m_fieldDescriptor;
}

bool PickerWidget::hasDescriptor() const
{
    return m_fieldDescriptor != std::nullopt;
}

void PickerWidget::setReadOnly(bool value)
{
    m_contentWidget->setEnabled(!value);
}

void PickerWidget::onDescriptorSet()
{
    m_label->setText(m_fieldDescriptor->displayName);
}

CommonParamsWidget* PickerWidget::parentParamsWidget() const
{
    return dynamic_cast<CommonParamsWidget*>(parent());
}

} // namespace nx::vms::client::desktop::rules
