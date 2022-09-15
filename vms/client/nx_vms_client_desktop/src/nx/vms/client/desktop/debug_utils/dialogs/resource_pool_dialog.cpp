// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_pool_dialog.h"

#include <QtWidgets/QBoxLayout>

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_context.h>

#include "../utils/debug_custom_actions.h"

namespace nx::vms::client::desktop {

ResourcePoolDialog::ResourcePoolDialog(QnResourcePool* resourcePool, QWidget* parent):
    base_type(parent)
{
    auto layout = new QVBoxLayout(this);
    layout->addWidget(new QnResourceListView(
        resourcePool->getResources(), QnResourceListView::SortByNameOption, this));
}

void ResourcePoolDialog::registerAction()
{
    registerDebugAction(
        "Resource Pool",
        [](QnWorkbenchContext* context)
        {
            auto dialog = std::make_unique<ResourcePoolDialog>(
                context->resourcePool(),
                context->mainWindowWidget());
            dialog->exec();
        });
}

} // namespace nx::vms::client::desktop
