#include "server_address_watcher.h"

#include <api/app_server_connection.h>

#include <network/module_finder.h>
#include <nx/network/socket_global.h>
#include <network/direct_module_finder.h>
#include <network/direct_module_finder_helper.h>
#include <client/client_message_processor.h>
#include <client_core/client_core_settings.h>

namespace
{
    const int kMaxUrlsToStore = 8;
}

QnServerAddressWatcher::QnServerAddressWatcher(
        QObject* parent)
    : QObject(parent)
{
    const auto moduleFinder = qnModuleFinder;
    NX_ASSERT(qnModuleFinder, "QnModuleFinder is not ready");
    if (!moduleFinder)
        return;

    const auto directModuleFinderHelper = moduleFinder->directModuleFinderHelper();
    NX_ASSERT(directModuleFinderHelper, "QnDirectModuleFinderHelper is not ready");
    if (!directModuleFinderHelper)
        return;

    const auto directModuleFinder = moduleFinder->directModuleFinder();

    m_urls = qnClientCoreSettings->knownServerUrls();
    if (m_urls.size() > kMaxUrlsToStore)
    {
        m_urls = m_urls.mid(0, kMaxUrlsToStore);
        qnClientCoreSettings->setKnownServerUrls(m_urls);
    }
    directModuleFinderHelper->setForcedUrls(this, QSet<QUrl>::fromList(m_urls));

    connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::connectionOpened,
            this, [this, directModuleFinderHelper, directModuleFinder]()
            {
                QUrl url = QnAppServerConnectionFactory::url();
                if (nx::network::SocketGlobals::addressResolver().isCloudHostName(url.host()))
                    return;

                url.setPath(QString());

                // Place url to the top of the list
                m_urls.removeOne(url);
                m_urls.prepend(url);
                while (m_urls.size() > kMaxUrlsToStore)
                    m_urls.removeLast();

                // Forces QnDirectModuleFinder to use url of connection
                directModuleFinder->checkUrl(url);

                directModuleFinderHelper->setForcedUrls(this, QSet<QUrl>::fromList(m_urls));

                qnClientCoreSettings->setKnownServerUrls(m_urls);
                qnClientCoreSettings->save();
            }
    );
}
