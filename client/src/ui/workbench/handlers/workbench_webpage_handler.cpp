#include "workbench_webpage_handler.h"

#include <client/client_message_processor.h>

#include <core/resource/webpage_resource.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>

#include <ui/dialogs/generic/input_dialog.h>

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

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this, []()
    {
        /* Online status is set by default, page will go offline if will be unreachable on opening. */
        for (const QnWebPageResourcePtr &webPage: qnResPool->getResources<QnWebPageResource>())
            webPage->setStatus(Qn::Online);
    });

    connect(action(QnActions::NewWebPageAction),   &QAction::triggered,        this,   &QnWorkbenchWebPageHandler::at_newWebPageAction_triggered);
}

QnWorkbenchWebPageHandler::~QnWorkbenchWebPageHandler()
{

}

void QnWorkbenchWebPageHandler::at_newWebPageAction_triggered()
{
    QScopedPointer<QnInputDialog> dialog(new QnInputDialog(mainWindow()));
    dialog->setWindowTitle(tr("New Web Page..."));
    dialog->setCaption(tr("Enter the url of the Web Page to add:"));
    dialog->setPlaceholderText(lit("example.org"));
    dialog->setWindowModality(Qt::ApplicationModal);

    while (true) {
         if(!dialog->exec())
             return;

        QUrl url = QUrl::fromUserInput(dialog->value());
        if (!isValidUrl(url))
            continue;

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
            Q_UNUSED(webPage);
        });

        break;
    };
}
