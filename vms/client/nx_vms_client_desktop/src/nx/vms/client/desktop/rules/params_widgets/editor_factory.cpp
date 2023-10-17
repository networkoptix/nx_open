// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "editor_factory.h"

#include "common_params_widget.h"

namespace nx::vms::client::desktop::rules {

ParamsWidget* EventEditorFactory::createWidget(
    const vms::rules::ItemDescriptor& descriptor,
    WindowContext* context,
    QWidget* parent)
{
    ParamsWidget* picker = new CommonParamsWidget(context, parent);

    picker->setDescriptor(descriptor);

    return picker;
}

ParamsWidget* ActionEditorFactory::createWidget(
    const vms::rules::ItemDescriptor& descriptor,
    WindowContext* context,
    QWidget* parent)
{
    ParamsWidget* picker = new CommonParamsWidget(context, parent);

    picker->setDescriptor(descriptor);

    return picker;
}

} // namespace nx::vms::client::desktop::rules
