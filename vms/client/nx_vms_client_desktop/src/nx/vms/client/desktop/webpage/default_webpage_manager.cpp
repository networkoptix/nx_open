// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "default_webpage_manager.h"

#include <core/resource/webpage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/url.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

namespace {

static const QString kDefaultWebPageQueryItem = "openAsDefault";

bool isDefaultWebPage(const QnWebPageResourcePtr& webpage)
{
    nx::utils::Url url(webpage->getUrl());
    return url.isValid() && url.hasQueryItem(kDefaultWebPageQueryItem);
}

} // namespace

DefaultWebpageManager::DefaultWebpageManager(QnWorkbenchContext* context, QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(context)
{
}

void DefaultWebpageManager::tryOpenDefaultWebPage()
{
    // Do nothing if session was restored.
    if (!workbench()->layouts().empty())
        return;

    for (const auto& webPage: resourcePool()->getResources<QnWebPageResource>())
    {
        if (isDefaultWebPage(webPage))
        {
            openDefaultWebPage(webPage);
            break;
        }
    }
}

void DefaultWebpageManager::openDefaultWebPage(const QnWebPageResourcePtr& webpage)
{
    menu()->trigger(ui::action::OpenInNewTabAction, webpage);

    if (NX_ASSERT(workbench()->currentLayout()->items().size() == 1))
    {
        QnWorkbenchItem* item = *workbench()->currentLayout()->items().begin();
        if (NX_ASSERT(item->resource() == webpage))
            workbench()->setItem(Qn::ZoomedRole, item);
    }
}

} // namespace nx::vms::client::desktop
