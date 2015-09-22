#include "workbench_resources_changes_watcher.h"

#include <common/common_module.h>

#include <core/resource/resource.h>
#include <core/resource_management/resources_changes_manager.h>

#include <ui/dialogs/resource_list_dialog.h>
#include <ui/workbench/workbench_context.h>

QnWorkbenchResourcesChangesWatcher::QnWorkbenchResourcesChangesWatcher(QObject *parent /*= nullptr*/):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(qnResourcesChangesManager, &QnResourcesChangesManager::saveChangesFailed, this, &QnWorkbenchResourcesChangesWatcher::showWarningDialog);
    connect(qnResourcesChangesManager, &QnResourcesChangesManager::resourceDeletingFailed, this, &QnWorkbenchResourcesChangesWatcher::showDeleteErrorDialog);
}

QnWorkbenchResourcesChangesWatcher::~QnWorkbenchResourcesChangesWatcher() {
}

void QnWorkbenchResourcesChangesWatcher::showWarningDialog(const QnResourceList &resources) {
    QString errorMessage = qnCommon->isReadOnly() 
        ? tr("The system is in Safe Mode.")
            + L'\n' 
            + tr("It is not allowed to make any changes except license activation.") 
            + L'\n' 
            + tr("The following %n items are not saved.", nullptr, resources.size())
        : tr("Could not save the following %n items to Server.", "", resources.size());

    QnResourceListDialog::exec(
        mainWindow(),
        resources,
        tr("Error"),
        errorMessage,
        QDialogButtonBox::Ok
        );
}

void QnWorkbenchResourcesChangesWatcher::showDeleteErrorDialog(const QnResourceList &resources) {
    QString errorMessage = qnCommon->isReadOnly() 
        ? tr("The system is in Safe Mode.")
        + L'\n' 
        + tr("It is not allowed to make any changes except license activation.") 
        + L'\n' 
        + tr("The following %n items are not deleted.", nullptr, resources.size())
        : tr("Could not delete the following %n items from Server.", "", resources.size());

    QnResourceListDialog::exec(
        mainWindow(),
        resources,
        tr("Error"),
        errorMessage,
        QDialogButtonBox::Ok
        );
}
