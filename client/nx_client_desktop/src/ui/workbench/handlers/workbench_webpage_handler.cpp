#include "workbench_webpage_handler.h"

#include <QtWidgets/QAction>

#include <common/common_module.h>

#include <client/client_message_processor.h>

#include <core/resource/webpage_resource.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_pool.h>

#include <nx/client/desktop/ui/actions/action_manager.h>

#include <ui/dialogs/webpage_dialog.h>

using namespace nx::client::desktop::ui;

QnWorkbenchWebPageHandler::QnWorkbenchWebPageHandler(QObject* parent /*= nullptr*/):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        [this]
        {
            // Online status by default, page will go offline if will be unreachable on opening.
            for (auto webPage: resourcePool()->getResources<QnWebPageResource>())
                webPage->setStatus(Qn::Online);
        });

    connect(action(action::NewWebPageAction), &QAction::triggered,
        this, &QnWorkbenchWebPageHandler::at_newWebPageAction_triggered);

    connect(action(action::WebPageSettingsAction), &QAction::triggered,
        this, &QnWorkbenchWebPageHandler::at_editWebPageAction_triggered);
}

QnWorkbenchWebPageHandler::~QnWorkbenchWebPageHandler()
{
}

void QnWorkbenchWebPageHandler::at_newWebPageAction_triggered()
{
    QScopedPointer<QnWebpageDialog> dialog(new QnWebpageDialog(mainWindowWidget()));
    dialog->setWindowTitle(tr("New Web Page"));
    if (!dialog->exec())
        return;

    const auto url = QUrl::fromUserInput(dialog->url());

    QnWebPageResourcePtr webPage(new QnWebPageResource(url));
    if (!dialog->name().isEmpty())
        webPage->setName(dialog->name());

    // No need to backup newly created webpage.
    auto applyChangesFunction = QnResourcesChangesManager::WebPageChangesFunction();
    auto callbackFunction =
        [this](bool success, const QnWebPageResourcePtr& webPage)
        {
            // Cannot capture the resource directly because real resource pointer may differ if the
            // transaction is received before the request callback.
            NX_EXPECT(webPage);
            if (success && webPage)
            {
                menu()->trigger(action::SelectNewItemAction, webPage);
                menu()->trigger(action::DropResourcesAction, webPage);
            }
        };

    qnResourcesChangesManager->saveWebPage(webPage, applyChangesFunction, callbackFunction);
}

void QnWorkbenchWebPageHandler::at_editWebPageAction_triggered()
{
    auto parameters = menu()->currentParameters(sender());

    auto webPage = parameters.resource().dynamicCast<QnWebPageResource>();
    if (!webPage)
        return;

    const auto oldName = webPage->getName();
    const auto oldUrl = webPage->getUrl();

    QScopedPointer<QnWebpageDialog> dialog(new QnWebpageDialog(mainWindowWidget()));
    dialog->setWindowTitle(tr("Edit Web Page"));
    dialog->setName(oldName);
    dialog->setUrl(oldUrl);
    if (!dialog->exec())
        return;

    if (oldName == dialog->name() && oldUrl == dialog->url())
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
