// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_settings_action_handler.h"

#include <QtGui/QAction>

#include <core/resource/user_resource.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_settings.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

ResourceTreeSettingsActionHandler::ResourceTreeSettingsActionHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    // Two actions have been introduced to implement single checkable 'Show servers' menu item,
    // one is supposed to always be checked, the other, in turn, is supposed to always be
    // unchecked. Since actions are created not on context menu request, but once on application
    // startup, straightforward implementation which use single action would be error prone:
    // there are would be many possible scenarios when checked state may became out of sync with
    // actual Resource Tree representation.

    connect(action(action::ShowServersInTreeAction), &QAction::triggered,
        this, &ResourceTreeSettingsActionHandler::showServersInResourceTree);

    connect(action(action::HideServersInTreeAction), &QAction::triggered,
        this, &ResourceTreeSettingsActionHandler::hideServersInResourceTree);
}

void ResourceTreeSettingsActionHandler::showServersInResourceTree()
{
    action(action::ShowServersInTreeAction)->setChecked(false); //< Keep unchecked state.
    context()->resourceTreeSettings()->setShowServersInTree(true);
}

void ResourceTreeSettingsActionHandler::hideServersInResourceTree()
{
    action(action::HideServersInTreeAction)->setChecked(true); //< Keep checked state.
    context()->resourceTreeSettings()->setShowServersInTree(false);
}

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
