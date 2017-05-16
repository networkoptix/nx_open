#include "server_address_watcher.h"

#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <nx/vms/discovery/manager.h>
#include <nx/network/socket_global.h>
#include <client/client_message_processor.h>
#include <nx/network/url/url_builder.h>

namespace {

const int kMaxUrlsToStore = 8;

} // namespace

QnServerAddressWatcher::QnServerAddressWatcher(QObject* parent):
    QObject(parent),
    QnCommonModuleAware(parent)
{
}

QnServerAddressWatcher::QnServerAddressWatcher(
    const Getter& getter,
    const Setter& setter,
    QObject* parent)
    :
    QnServerAddressWatcher(parent)
{
    setAccessors(getter, setter);
}

void QnServerAddressWatcher::setAccessors(const Getter& getter, const Setter& setter)
{
    if (!getter || !setter)
        return;

    using namespace nx::utils;

    const auto moduleManager = commonModule()->moduleDiscoveryManager();
    NX_ASSERT(moduleManager);
    if (!moduleManager)
        return;

    m_urls = getter();
    if (m_urls.size() > kMaxUrlsToStore)
    {
        shrinkUrlsList();
        setter(m_urls);
    }

    // TODO: Adding url without expected server id leads to a one time check.
    // Might make some problems if there is no internet connection at the momment.
    for (const auto& url: m_urls)
        moduleManager->checkEndpoint(url);

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionOpened,
        this, [this, moduleManager, setter]()
        {
            QUrl url = commonModule()->currentUrl();
            if (nx::network::SocketGlobals::addressResolver().isCloudHostName(url.host()))
                return;

            url = nx::network::url::Builder()
                .setScheme(lit("http"))
                .setHost(url.host())
                .setPort(url.port());

            // Place url to the top of the list.
            m_urls.removeOne(url);
            m_urls.prepend(url);
            shrinkUrlsList();

            moduleManager->checkEndpoint(url, commonModule()->remoteGUID());
            setter(m_urls);
        }
    );
}

void QnServerAddressWatcher::shrinkUrlsList()
{
    if (m_urls.size() > kMaxUrlsToStore)
        m_urls.erase(m_urls.begin() + kMaxUrlsToStore, m_urls.end());
}
