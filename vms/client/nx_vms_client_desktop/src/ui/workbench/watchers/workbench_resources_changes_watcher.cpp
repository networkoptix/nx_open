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
    const auto extras = (commonModule()->isReadOnly()
        ? tr("System is in Safe Mode. It is not allowed "
            "to make any changes except license activation.")
            + L'\n' + tr("The following %n items are not saved:", "", resources.size())

        : tr("The following %n items are not saved:", "", resources.size()));

    const auto icon = static_cast<QnMessageBoxIcon>(commonModule()->isReadOnly()
        ? QnMessageBoxIcon::Warning
        : QnMessageBoxIcon::Critical);

    const auto text = (commonModule()->isReadOnly()
        ? tr("Changing System configuration not allowed in Safe Mode")
        : tr("Failed to save changes"));

    QnMessageBox dialog(icon, text, extras,
        QDialogButtonBox::Ok, QDialogButtonBox::Ok,
        mainWindowWidget());
    dialog.addCustomWidget(new QnResourceListView(resources, &dialog));
    dialog.exec();
}

void QnWorkbenchResourcesChangesWatcher::showDeleteErrorDialog(const QnResourceList& resources)
{
    const auto extras = (commonModule()->isReadOnly()
        ? tr("System is in Safe Mode. It is not allowed to "
            "make any changes except license activation.")
        + L'\n' + tr("The following %n items are not deleted:", "", resources.size())

        : QString());

    const auto icon = static_cast<QnMessageBoxIcon>(commonModule()->isReadOnly()
        ? QnMessageBoxIcon::Warning
        : QnMessageBoxIcon::Critical);

    const auto text = (commonModule()->isReadOnly()
        ? tr("Deleting objects not allowed in Safe Mode")
        : tr("Failed to delete %n items:", "", resources.size()));

    QnMessageBox dialog(icon, text, extras,
        QDialogButtonBox::Ok, QDialogButtonBox::Ok,
        mainWindowWidget());
    dialog.addCustomWidget(new QnResourceListView(resources, &dialog));
    dialog.exec();
}
