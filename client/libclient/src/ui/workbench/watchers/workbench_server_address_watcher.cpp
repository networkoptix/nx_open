#include "workbench_server_address_watcher.h"

#include <api/app_server_connection.h>

#include <network/module_finder.h>
#include <network/direct_module_finder_helper.h>
#include <client/client_message_processor.h>
#include <client/client_settings.h>
#include <client_core/client_core_settings.h>

namespace
{
    const int kMaxUrlsToStore = 8;
}

QnWorkbenchServerAddressWatcher::QnWorkbenchServerAddressWatcher(
        QObject* parent)
    : QObject(parent)
    , QnWorkbenchContextAware(parent)
{
    auto moduleFinder = qnModuleFinder;
    if (!moduleFinder)
        return;

    auto directModuleFinderHelper = moduleFinder->directModuleFinderHelper();
    if (!directModuleFinderHelper)
        return;

    m_urls = qnSettings->knownServerUrls();
    if (m_urls.size() > kMaxUrlsToStore)
    {
        m_urls = m_urls.mid(0, kMaxUrlsToStore);
        qnSettings->setKnownServerUrls(m_urls);
    }
    directModuleFinderHelper->setForcedUrls(QSet<QUrl>::fromList(m_urls));

    connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::connectionOpened,
            this, [this, directModuleFinderHelper]()
            {
                QUrl url = QnAppServerConnectionFactory::url();

                const bool isLocalConnection =
                    [url]() -> bool
                    {
                        /*
                        * We save only local connection to the recent list.
                        * So, we check if last saved connection is current.
                        * If not - it means it was connection through the cloud
                        * and we don't save it.
                        */

                        const auto localConnections = qnClientCoreSettings->recentLocalConnections();
                        if (localConnections.isEmpty())
                            return false;

                        return (url.host() == localConnections.first().url.host());
                    }();

                if (!isLocalConnection)
                    return;


                url.setPath(QString());

                // Place url to the top of the list
                m_urls.removeOne(url);
                m_urls.prepend(url);
                while (m_urls.size() > kMaxUrlsToStore)
                    m_urls.removeLast();

                directModuleFinderHelper->setForcedUrls(QSet<QUrl>::fromList(m_urls));

                qnSettings->setKnownServerUrls(m_urls);
            }
    );
}
