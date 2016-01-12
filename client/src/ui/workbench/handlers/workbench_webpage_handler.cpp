#include "workbench_webpage_handler.h"

#include <core/resource/webpage_resource.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>

#include <ui/dialogs/layout_name_dialog.h>

namespace
{
    const static QString kHttp(lit("http"));
    const static QString kHttps(lit("https"));

    bool isValidUrl(const QUrl &url) {
        if (!url.isValid())
            return false;

        return url.scheme() == kHttp || url.scheme() == kHttps;
    }
}

QnWorkbenchWebPageHandler::QnWorkbenchWebPageHandler(QObject *parent /*= 0*/)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
{
    connect(action(Qn::NewWebPageAction),   &QAction::triggered,        this,   &QnWorkbenchWebPageHandler::at_newWebPageAction_triggered);
}

QnWorkbenchWebPageHandler::~QnWorkbenchWebPageHandler()
{

}

void QnWorkbenchWebPageHandler::at_newWebPageAction_triggered()
{
    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, mainWindow()));
    dialog->setWindowTitle(tr("New Web Page..."));
    dialog->setText(tr("Enter the url of the Web Page to add:"));
    dialog->setWindowModality(Qt::ApplicationModal);

    while (true) {
        if(!dialog->exec())
            return;

        //TODO: #GDM create dialog with correct validation support
        QUrl url = QUrl::fromUserInput(dialog->name().trimmed());

        if (!isValidUrl(url))
        {
            continue;
        }


        QnWebPageResourcePtr webPage(new QnWebPageResource(url));
        if (qnResPool->getResourceById(webPage->getId()))
        {
            QMessageBox::warning(
                mainWindow(),
                tr("Web Page already exists."),
                tr("This Web Page is already exists.")
                );
            continue;
        }

        qnResourcesChangesManager->saveWebPage(webPage, [](const QnWebPageResourcePtr &webPage)
        {
            webPage->setStatus(Qn::Online);
        });

        break;
    };
}
