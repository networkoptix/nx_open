#include "workbench_webpage_handler.h"

#include <client/client_message_processor.h>

#include <core/resource/webpage_resource.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>

#include <ui/dialogs/webpage_dialog.h>

QnWorkbenchWebPageHandler::QnWorkbenchWebPageHandler(QObject* parent /*= nullptr*/):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        []()
        {
            /* Online status is set by default, page will go offline if will be unreachable on opening. */
            for (const auto& webPage: qnResPool->getResources<QnWebPageResource>())
                webPage->setStatus(Qn::Online);
        });

    connect(action(QnActions::NewWebPageAction), &QAction::triggered,
        this, &QnWorkbenchWebPageHandler::at_newWebPageAction_triggered);

    connect(action(QnActions::EditResourceAction), &QAction::triggered,
        this, &QnWorkbenchWebPageHandler::at_editWebPageAction_triggered);
}

QnWorkbenchWebPageHandler::~QnWorkbenchWebPageHandler()
{
}

void QnWorkbenchWebPageHandler::at_newWebPageAction_triggered()
{
    QScopedPointer<QnWebpageDialog> dialog(new QnWebpageDialog(mainWindow()));
    dialog->setWindowTitle(tr("New Web Page"));
    if (!dialog->exec())
        return;

    const auto url = QUrl::fromUserInput(dialog->url());

    QnWebPageResourcePtr webPage(new QnWebPageResource(url));
    if (!dialog->name().isEmpty())
        webPage->setName(dialog->name());

    qnResourcesChangesManager->saveWebPage(webPage, [](const QnWebPageResourcePtr& /*webPage*/) {});
}

void QnWorkbenchWebPageHandler::at_editWebPageAction_triggered()
{
    auto parameters = menu()->currentParameters(sender());

    auto webPage = parameters.resource().dynamicCast<QnWebPageResource>();
    if (!webPage)
        return;

    QScopedPointer<QnWebpageDialog> dialog(new QnWebpageDialog(mainWindow()));
    dialog->setWindowTitle(tr("Edit Web Page"));
    dialog->setName(webPage->getName());
    dialog->setUrl(webPage->getUrl());
    if (!dialog->exec())
        return;

    const auto url = QUrl::fromUserInput(dialog->url());

    qnResourcesChangesManager->saveWebPage(webPage,
        [url, name = dialog->name()](const QnWebPageResourcePtr& webPage)
        {
            webPage->setUrl(url.toString());
            webPage->setName(name.isEmpty()
                ? QnWebPageResource::nameForUrl(url)
                : name);
        });
}
