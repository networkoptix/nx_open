#include "server_address_watcher.h"

#include <api/app_server_connection.h>

#include <network/module_finder.h>
#include <nx/network/socket_global.h>
#include <network/direct_module_finder.h>
#include <network/direct_module_finder_helper.h>
#include <client/client_message_processor.h>
#include <nx/utils/url_builder.h>

namespace {

const int kMaxUrlsToStore = 8;

} // namespace

QnServerAddressWatcher::QnServerAddressWatcher(QObject* parent):
    QObject(parent)
{
}

QnServerAddressWatcher::QnServerAddressWatcher(
    const Getter& getter,
    const Setter& setter,
    QObject* parent)
    :
    QObject(parent)
{
    setAccessors(getter, setter);
}

void QnServerAddressWatcher::setAccessors(const Getter& getter, const Setter& setter)
{
    if (!getter || !setter)
        return;

    using namespace nx::utils;

    const auto moduleFinder = qnModuleFinder;
    NX_ASSERT(qnModuleFinder, "QnModuleFinder is not ready");
    if (!moduleFinder)
        return;

    const auto directModuleFinderHelper = moduleFinder->directModuleFinderHelper();
    NX_ASSERT(directModuleFinderHelper, "QnDirectModuleFinderHelper is not ready");
    if (!directModuleFinderHelper)
        return;

    const auto directModuleFinder = moduleFinder->directModuleFinder();

    m_urls = getter();
    if (m_urls.size() > kMaxUrlsToStore)
    {
        shrinkUrlsList();
        setter(m_urls);
    }
    directModuleFinderHelper->setForcedUrls(this, QSet<QUrl>::fromList(m_urls));

    for (const auto& url: m_urls)
        directModuleFinder->checkUrl(url);

    connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::connectionOpened,
        this, [this, directModuleFinderHelper, directModuleFinder, setter]()
        {
            QUrl url = QnAppServerConnectionFactory::url();
            if (nx::network::SocketGlobals::addressResolver().isCloudHostName(url.host()))
                return;

            url = UrlBuilder()
                .setScheme(lit("http"))
                .setHost(url.host())
                .setPort(url.port());

            // Place url to the top of the list.
            m_urls.removeOne(url);
            m_urls.prepend(url);
            shrinkUrlsList();

            // Forces QnDirectModuleFinder to use url of connection
            directModuleFinder->checkUrl(url);

            directModuleFinderHelper->setForcedUrls(this, QSet<QUrl>::fromList(m_urls));

            setter(m_urls);
        }
    );
}

void QnServerAddressWatcher::shrinkUrlsList()
{
    if (m_urls.size() > kMaxUrlsToStore)
        m_urls.erase(m_urls.begin() + kMaxUrlsToStore, m_urls.end());
}
