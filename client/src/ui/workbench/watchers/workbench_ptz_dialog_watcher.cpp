#include "workbench_ptz_dialog_watcher.h"

#include <core/resource/resource.h>

#include <ui/dialogs/ptz_manage_dialog.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench.h>

QnWorkbenchPtzDialogWatcher::QnWorkbenchPtzDialogWatcher(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(workbench(),    &QnWorkbench::currentLayoutAboutToBeChanged,    this,   &QnWorkbenchPtzDialogWatcher::at_workbench_currentLayoutAboutToBeChanged);
    connect(workbench(),    &QnWorkbench::currentLayoutChanged,             this,   &QnWorkbenchPtzDialogWatcher::at_workbench_currentLayoutChanged);
}

void QnWorkbenchPtzDialogWatcher::at_workbench_currentLayoutChanged() {
    connect(workbench()->currentLayout(), &QnWorkbenchLayout::itemRemoved, this, &QnWorkbenchPtzDialogWatcher::at_currentLayout_itemRemoved);
}

void QnWorkbenchPtzDialogWatcher::at_workbench_currentLayoutAboutToBeChanged() {
    disconnect(workbench()->currentLayout(), &QnWorkbenchLayout::itemRemoved, this, &QnWorkbenchPtzDialogWatcher::at_currentLayout_itemRemoved);
    closePtzManageDialog();
}

void QnWorkbenchPtzDialogWatcher::at_currentLayout_itemRemoved(QnWorkbenchItem *item) {
    closePtzManageDialog(item);
}

void QnWorkbenchPtzDialogWatcher::closePtzManageDialog(QnWorkbenchItem *item) {
    QnPtzManageDialog *dialog = QnPtzManageDialog::instance();
    if (!dialog)
        return;

    if (!dialog->isVisible() || dialog->resource().isNull())
        return;

    if (item && item->resourceUid() != dialog->resource()->getUniqueId())
        return;

    if (dialog->isModified())
        dialog->askToSaveChanges(false);
    dialog->hide();
}
