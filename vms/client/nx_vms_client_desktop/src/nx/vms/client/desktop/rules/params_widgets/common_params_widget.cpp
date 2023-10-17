// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_params_widget.h"

#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/rules/picker_widgets/picker_factory.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/workbench/workbench_context.h>

#include "../dialog_details/dotted_line.h"
#include "../picker_widgets/plain_picker_widget.h"

namespace nx::vms::client::desktop::rules {

CommonParamsWidget::CommonParamsWidget(WindowContext* context, QWidget* parent):
    ParamsWidget(context, parent)
{
}

void CommonParamsWidget::onDescriptorSet()
{
    auto layout = new QVBoxLayout;
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_pickers.clear();

    bool isPreviousPlain{true};
    for (const auto& fieldDescriptor: descriptor().fields)
    {
        if (!fieldDescriptor.properties.value("visible", true).toBool())
            continue;

        PickerWidget* picker = PickerFactory::createWidget(fieldDescriptor, windowContext(), this);
        if (picker == nullptr)
            continue;

        const bool isCurrentPlain = dynamic_cast<PlainPickerWidget*>(picker) != nullptr;
        if (!isCurrentPlain || !isPreviousPlain)
        {
            layout->addSpacing(4);

            auto dottedLine = new DottedLineWidget;
            layout->addWidget(dottedLine);
        }

        isPreviousPlain = isCurrentPlain;

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
