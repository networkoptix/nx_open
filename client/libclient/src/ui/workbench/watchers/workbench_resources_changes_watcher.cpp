#include "workbench_resources_changes_watcher.h"

#include <common/common_module.h>

#include <core/resource/resource.h>
#include <core/resource_management/resources_changes_manager.h>

#include <ui/help/help_topics.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_context.h>

QnWorkbenchResourcesChangesWatcher::QnWorkbenchResourcesChangesWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(qnResourcesChangesManager, &QnResourcesChangesManager::saveChangesFailed, this,
        &QnWorkbenchResourcesChangesWatcher::showWarningDialog);
    connect(qnResourcesChangesManager, &QnResourcesChangesManager::resourceDeletingFailed, this,
        &QnWorkbenchResourcesChangesWatcher::showDeleteErrorDialog);
}

QnWorkbenchResourcesChangesWatcher::~QnWorkbenchResourcesChangesWatcher() {
}

void QnWorkbenchResourcesChangesWatcher::showWarningDialog(const QnResourceList& resources)
{
    const auto message = qnCommon->isReadOnly()
        ? tr("The system is in Safe Mode.")
            + L'\n'
            + tr("It is not allowed to make any changes except license activation.")
            + L'\n'
            + tr("The following %n items are not saved.", "", resources.size())
        : tr("Could not save the following %n items to Server.", "", resources.size());

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Error"),
        tr("Error while saving changes"),
        QDialogButtonBox::Ok,
        mainWindow());
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(message);
    messageBox.addCustomWidget(new QnResourceListView(resources));
    messageBox.exec();
}

void QnWorkbenchResourcesChangesWatcher::showDeleteErrorDialog(const QnResourceList& resources)
{
    const auto message = qnCommon->isReadOnly()
        ? tr("The system is in Safe Mode.")
        + L'\n'
        + tr("It is not allowed to make any changes except license activation.")
        + L'\n'
        + tr("The following %n items are not deleted.", "", resources.size())
        : tr("Could not delete the following %n items from Server.", "", resources.size());

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Error"),
        tr("Error while deleting items"),
        QDialogButtonBox::Ok,
        mainWindow());
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(message);
    messageBox.addCustomWidget(new QnResourceListView(resources));
    messageBox.exec();
}
