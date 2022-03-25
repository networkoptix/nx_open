// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_webpage_handler.h"

#include <QtWidgets/QAction>

#include <common/common_module.h>

#include <client/client_globals.h>
#include <client/client_message_processor.h>

#include <core/resource/webpage_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>

#include <ui/dialogs/webpage_dialog.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

QnWorkbenchWebPageHandler::QnWorkbenchWebPageHandler(QObject* parent /*= nullptr*/):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        [this]
        {
            // Online status by default, page will go offline if will be unreachable on opening.
            for (auto webPage: resourcePool()->getResources<QnWebPageResource>())
                webPage->setStatus(nx::vms::api::ResourceStatus::online);
        });

    connect(action(action::AddProxiedWebPageAction), &QAction::triggered,
        this, &QnWorkbenchWebPageHandler::at_newWebPageAction_triggered);

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
    QScopedPointer<QnWebpageDialog> dialog(
        new QnWebpageDialog(mainWindowWidget(), QnWebpageDialog::AddPage));

    const auto params = menu()->currentParameters(sender());
    if (const auto server = params.resource().dynamicCast<QnMediaServerResource>())
        dialog->setProxyId(server->getId());

    if (!dialog->exec())
        return;

    QnWebPageResourcePtr webPage(new QnWebPageResource());
    webPage->setIdUnsafe(QnUuid::createUuid());
    webPage->setUrl(dialog->url().toString());
    webPage->setName(dialog->name());
    resourcePool()->addResource(webPage);

    // Properties can be set only after Resource is added to the Resource Pool.
    webPage->setSubtype(dialog->subtype());
    webPage->setProxyId(dialog->proxyId());
    webPage->setProxyDomainAllowList(dialog->proxyDomainAllowList());
    webPage->setCertificateCheckEnabled(dialog->isCertificateCheckEnabled());

    qnResourcesChangesManager->saveWebPage(webPage);
    menu()->trigger(action::SelectNewItemAction, webPage);
    menu()->trigger(action::DropResourcesAction, webPage);
}

void QnWorkbenchWebPageHandler::at_editWebPageAction_triggered()
{
    auto parameters = menu()->currentParameters(sender());

    auto webPage = parameters.resource().dynamicCast<QnWebPageResource>();
    if (!webPage)
        return;

    const auto oldName = webPage->getName();
    const auto oldUrl = webPage->getUrl();
    const auto oldProxy = webPage->getProxyId();
    const auto oldSubType = webPage->subtype();
    const auto oldProxyDomainAllowList = webPage->proxyDomainAllowList();
    const auto oldCertCheck = webPage->isCertificateCheckEnabled();

    QScopedPointer<QnWebpageDialog> dialog(
        new QnWebpageDialog(mainWindowWidget(), QnWebpageDialog::EditPage));

    dialog->setName(oldName);
    dialog->setUrl(oldUrl);
    dialog->setProxyId(oldProxy);
    dialog->setSubtype(oldSubType);
    dialog->setProxyDomainAllowList(oldProxyDomainAllowList);
    dialog->setCertificateCheckEnabled(oldCertCheck);

    dialog->setResourceId(webPage->getId());

    if (!dialog->exec())
        return;

    const auto url = dialog->url();
    const auto name = dialog->name();
    const auto proxy = dialog->proxyId();
    const auto subtype = dialog->subtype();
    const auto proxyDomainAllowList = dialog->proxyDomainAllowList();
    const auto certCheck = dialog->isCertificateCheckEnabled();

    if (oldName == name
        && oldUrl == url.toString()
        && oldProxy == proxy
        && oldSubType == subtype
        && proxyDomainAllowList == oldProxyDomainAllowList
        && oldCertCheck == certCheck)
    {
        return;
    }

    qnResourcesChangesManager->saveWebPage(webPage,
        [url, name, proxy, subtype, proxyDomainAllowList, certCheck](
            const QnWebPageResourcePtr& webPage)
        {
            webPage->setUrl(url.toString());
            webPage->setName(name);
            webPage->setProxyId(proxy);
            webPage->setSubtype(subtype);
            webPage->setProxyDomainAllowList(proxyDomainAllowList);
            webPage->setCertificateCheckEnabled(certCheck);
        });
}
