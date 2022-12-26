// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_params_widget.h"

#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/rules/picker_widgets/picker_factory.h>
#include <nx/vms/client/desktop/style/helper.h>

namespace nx::vms::client::desktop::rules {

CommonParamsWidget::CommonParamsWidget(SystemContext* context, QWidget* parent):
    ParamsWidget(context, parent)
{
}

void CommonParamsWidget::onDescriptorSet()
{
    auto layout = new QVBoxLayout;
    layout->setSpacing(style::Metrics::kDefaultLayoutSpacing.height());
    layout->setContentsMargins(0, 0, 0, 0);

    for (const auto& fieldDescriptor: descriptor().fields)
    {
        PickerWidget* picker = PickerFactory::createWidget(fieldDescriptor, systemContext());
        layout->addWidget(picker);
    }

    auto verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    layout->addItem(verticalSpacer);

    setLayout(layout);
}

} // namespace nx::vms::client::desktop::rules
