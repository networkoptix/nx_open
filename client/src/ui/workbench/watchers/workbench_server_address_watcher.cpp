#include "workbench_server_address_watcher.h"

#include <api/app_server_connection.h>

#include "utils/network/module_finder.h"
#include "utils/network/direct_module_finder_helper.h"
#include "utils/network/module_information.h"
#include "client/client_message_processor.h"
#include "client/client_settings.h"


QnWorkbenchServerAddressWatcher::QnWorkbenchServerAddressWatcher(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    QnModuleFinder *moduleFinder = QnModuleFinder::instance();
    if (!moduleFinder)
        return;

    m_urls = QSet<QUrl>::fromList(qnSettings->knownServerUrls());

    connect(moduleFinder, &QnModuleFinder::moduleAddressFound, this, &QnWorkbenchServerAddressWatcher::at_moduleFinder_moduleUrlFound);
    moduleFinder->directModuleFinderHelper()->setForcedUrls(m_urls);

    connect(
        QnClientMessageProcessor::instance(), &QnClientMessageProcessor::connectionOpened,
        this, &QnWorkbenchServerAddressWatcher::at_ec_connection_established );
}

void QnWorkbenchServerAddressWatcher::at_moduleFinder_moduleUrlFound(const QnModuleInformation &moduleInformation, const SocketAddress &address) {
    Q_UNUSED(moduleInformation)

    QUrl url;
    url.setScheme(lit("http"));
    url.setHost(address.address.toString());
    url.setPort(address.port);

    if (m_urls.contains(url))
        return;

    m_urls.insert(url);
    qnSettings->setKnownServerUrls(m_urls.toList());
    QnModuleFinder::instance()->directModuleFinderHelper()->setForcedUrls(m_urls);
}

void QnWorkbenchServerAddressWatcher::at_ec_connection_established()
{
    QUrl url = QnAppServerConnectionFactory::url();
    url.setPath( QString() );

    auto dmf = QnModuleFinder::instance()->directModuleFinderHelper();
    dmf->addForcedUrl( std::move( url ) );
}
