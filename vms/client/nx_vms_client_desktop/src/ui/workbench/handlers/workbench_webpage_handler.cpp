// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_webpage_handler.h"

#include <quazip/JlCompress.h>
#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtGui/QAction>
#include <QtGui/QDesktopServices>

#include <client/client_globals.h>
#include <client/client_message_processor.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <ui/dialogs/webpage_dialog.h>

using namespace nx::vms::client::desktop;

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

    connect(action(menu::AddProxiedWebPageAction), &QAction::triggered,
        this, [this] { addNewWebPage(nx::vms::api::WebPageSubtype::none); });

    connect(action(menu::NewWebPageAction), &QAction::triggered,
        this, [this] { addNewWebPage(nx::vms::api::WebPageSubtype::none); });

    connect(action(menu::WebPageSettingsAction), &QAction::triggered,
        this, &QnWorkbenchWebPageHandler::editWebPage);

    if (ini().webPagesAndIntegrations)
    {
        connect(action(menu::IntegrationSettingsAction), &QAction::triggered,
            this, &QnWorkbenchWebPageHandler::editWebPage);

        connect(action(menu::AddProxiedIntegrationAction), &QAction::triggered,
            this, [this] { addNewWebPage(nx::vms::api::WebPageSubtype::clientApi); });

        connect(action(menu::NewIntegrationAction), &QAction::triggered,
            this, [this] { addNewWebPage(nx::vms::api::WebPageSubtype::clientApi); });
    }

    connect(action(menu::OpenJavaScriptApiDocumentation), &QAction::triggered, this,
        []()
        {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(false);

            QFile doc(":/js_api_doc.zip");
            JlCompress::extractDir(&doc, tempDir.path());

            const QString filePath = tempDir.filePath("index.html");

            if (QFile::exists(filePath))
                QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        });

}

QnWorkbenchWebPageHandler::~QnWorkbenchWebPageHandler()
{
}

void QnWorkbenchWebPageHandler::addNewWebPage(nx::vms::api::WebPageSubtype subtype)
{
    QScopedPointer<QnWebpageDialog> dialog(
        new QnWebpageDialog(mainWindowWidget(), QnWebpageDialog::AddPage));

    const auto params = menu()->currentParameters(sender());
    if (params.size() > 0)
    {
        const auto server = params.resource().dynamicCast<QnMediaServerResource>();
        dialog->setProxyId(server->getId());
    }

    dialog->setSubtype(subtype);

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
    webPage->setRefreshInterval(dialog->refreshInterval());

    qnResourcesChangesManager->saveWebPage(webPage);
    menu()->trigger(menu::SelectNewItemAction, webPage);
    menu()->trigger(menu::DropResourcesAction, webPage);
}

void QnWorkbenchWebPageHandler::editWebPage()
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
    const auto oldRefreshInterval = webPage->refreshInterval();

    QScopedPointer<QnWebpageDialog> dialog(
        new QnWebpageDialog(mainWindowWidget(), QnWebpageDialog::EditPage));

    dialog->setName(oldName);
    dialog->setUrl(oldUrl);
    dialog->setProxyId(oldProxy);
    dialog->setSubtype(oldSubType);
    dialog->setProxyDomainAllowList(oldProxyDomainAllowList);
    dialog->setCertificateCheckEnabled(oldCertCheck);
    dialog->setRefreshInterval(oldRefreshInterval);

    dialog->setResourceId(webPage->getId());

    if (!dialog->exec())
        return;

    const auto url = dialog->url();
    const auto name = dialog->name();
    const auto proxy = dialog->proxyId();
    const auto subtype = dialog->subtype();
    const auto proxyDomainAllowList = dialog->proxyDomainAllowList();
    const auto certCheck = dialog->isCertificateCheckEnabled();
    const auto refreshInterval = dialog->refreshInterval();

    if (oldName == name
        && oldUrl == url.toString()
        && oldProxy == proxy
        && oldSubType == subtype
        && proxyDomainAllowList == oldProxyDomainAllowList
        && oldCertCheck == certCheck
        && oldRefreshInterval == refreshInterval)
    {
        return;
    }

    webPage->setUrl(url.toString());
    webPage->setName(name);
    webPage->setProxyId(proxy);
    webPage->setSubtype(subtype);
    webPage->setProxyDomainAllowList(proxyDomainAllowList);
    webPage->setCertificateCheckEnabled(certCheck);
    webPage->setRefreshInterval(refreshInterval);
    qnResourcesChangesManager->saveWebPage(webPage);
}
