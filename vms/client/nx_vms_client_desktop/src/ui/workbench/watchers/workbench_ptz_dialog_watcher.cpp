// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_ptz_dialog_watcher.h"

#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/dialogs/ptz_manage_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_item.h>

using namespace nx::vms::client::desktop;

QnWorkbenchPtzDialogWatcher::QnWorkbenchPtzDialogWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(workbench(), &Workbench::currentLayoutAboutToBeChanged, this,
        [this]() { closePtzManageDialog(); });
    connect(workbench(), &Workbench::currentLayoutItemRemoved, this,
        &QnWorkbenchPtzDialogWatcher::closePtzManageDialog);
}

void QnWorkbenchPtzDialogWatcher::closePtzManageDialog(QnWorkbenchItem* item)
{
    auto dialog = QnPtzManageDialog::instance();
    if (!dialog || !dialog->isVisible() || dialog->widget().isNull())
        return;

    if (!item || dialog->widget()->item() == item)
    {
        if (dialog->isModified())
            dialog->askToSaveChanges(false);
        dialog->hide();
    }
}
