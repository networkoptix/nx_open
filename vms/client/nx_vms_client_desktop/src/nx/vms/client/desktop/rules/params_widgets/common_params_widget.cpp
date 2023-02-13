// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_params_widget.h"

#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/rules/picker_widgets/picker_factory.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop::rules {

CommonParamsWidget::CommonParamsWidget(QnWorkbenchContext* context, QWidget* parent):
    ParamsWidget(context, parent)
{
}

void CommonParamsWidget::onDescriptorSet()
{
    auto layout = new QVBoxLayout;
    layout->setSpacing(style::Metrics::kDefaultLayoutSpacing.height());
    layout->setContentsMargins(0, 0, 0, 0);

    m_pickers.clear();
    for (const auto& fieldDescriptor: descriptor().fields)
    {
        PickerWidget* picker = PickerFactory::createWidget(fieldDescriptor, context(), this);
        m_pickers.push_back(picker);
        layout->addWidget(picker);
    }

    auto verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    layout->addItem(verticalSpacer);

    setLayout(layout);
}

void CommonParamsWidget::updateUi()
{
    for (auto picker: m_pickers)
        picker->updateUi();
}

} // namespace nx::vms::client::desktop::rules
