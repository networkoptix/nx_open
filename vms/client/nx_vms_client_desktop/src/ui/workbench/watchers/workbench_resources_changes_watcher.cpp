// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_resources_changes_watcher.h"

#include <common/common_module.h>
#include <core/resource/resource.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::desktop;

namespace {

bool isUserVisibleResource(const QnResourcePtr& resource)
{
    return !resource->getName().isEmpty()
        && !resource->hasFlags(Qn::desktop_camera);
}

} // namespace

QnWorkbenchResourcesChangesWatcher::QnWorkbenchResourcesChangesWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(qnResourcesChangesManager, &ResourcesChangesManager::saveChangesFailed, this,
        &QnWorkbenchResourcesChangesWatcher::showWarningDialog);
    connect(qnResourcesChangesManager, &ResourcesChangesManager::resourceDeletingFailed, this,
        &QnWorkbenchResourcesChangesWatcher::showDeleteErrorDialog);
}

QnWorkbenchResourcesChangesWatcher::~QnWorkbenchResourcesChangesWatcher() {
}

void QnWorkbenchResourcesChangesWatcher::showWarningDialog(const QnResourceList& resources)
{
    const auto visibleResources = resources.filtered(isUserVisibleResource);
    if (visibleResources.empty())
        return;

    const auto extras = tr("The following %n items are not saved:", "", visibleResources.size());
    const auto icon = QnMessageBoxIcon::Critical;
    const auto text = tr("Failed to save changes");

    QnMessageBox dialog(icon, text, extras,
        QDialogButtonBox::Ok, QDialogButtonBox::Ok,
        mainWindowWidget());
    dialog.addCustomWidget(new QnResourceListView(visibleResources, &dialog));
    dialog.exec();
}

void QnWorkbenchResourcesChangesWatcher::showDeleteErrorDialog(const QnResourceList& resources)
{
    const auto visibleResources = resources.filtered(isUserVisibleResource);
    if (visibleResources.empty())
        return;

    const auto icon = QnMessageBoxIcon::Critical;

    const auto text = tr("Failed to delete %n items:", "", visibleResources.size());

    QnMessageBox dialog(icon, text, /*extras*/ QString(),
        QDialogButtonBox::Ok, QDialogButtonBox::Ok,
        mainWindowWidget());
    dialog.addCustomWidget(new QnResourceListView(visibleResources, &dialog));
    dialog.exec();
}
