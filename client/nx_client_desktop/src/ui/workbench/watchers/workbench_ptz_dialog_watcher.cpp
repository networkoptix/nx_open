#include "workbench_ptz_dialog_watcher.h"

#include <core/resource/resource.h>

#include <ui/dialogs/ptz_manage_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench.h>

QnWorkbenchPtzDialogWatcher::QnWorkbenchPtzDialogWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged, this,
        &QnWorkbenchPtzDialogWatcher::at_workbench_currentLayoutAboutToBeChanged);
    connect(workbench(), &QnWorkbench::currentLayoutChanged, this,
        &QnWorkbenchPtzDialogWatcher::at_workbench_currentLayoutChanged);
}

void QnWorkbenchPtzDialogWatcher::at_workbench_currentLayoutChanged()
{
    connect(workbench()->currentLayout(), &QnWorkbenchLayout::itemRemoved, this,
        &QnWorkbenchPtzDialogWatcher::at_currentLayout_itemRemoved);
}

void QnWorkbenchPtzDialogWatcher::at_workbench_currentLayoutAboutToBeChanged()
{
    workbench()->currentLayout()->disconnect(this);
    closePtzManageDialog();
}

void QnWorkbenchPtzDialogWatcher::at_currentLayout_itemRemoved(QnWorkbenchItem* item)
{
    closePtzManageDialog(item);
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
