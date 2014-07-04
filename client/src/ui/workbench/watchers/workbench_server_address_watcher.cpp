#include "workbench_server_address_watcher.h"

#include "utils/network/direct_module_finder.h"
#include "utils/network/direct_module_finder_helper.h"
#include "client/client_settings.h"

QnWorkbenchServerAddressWatcher::QnWorkbenchServerAddressWatcher(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_directModuleFinder(0),
    m_directModuleFinderHelper(0)
{
    m_urls = QSet<QUrl>::fromList(qnSettings->knownServerUrls());
}

void QnWorkbenchServerAddressWatcher::setDirectModuleFinder(QnDirectModuleFinder *directModuleFinder) {
    if (m_directModuleFinder == directModuleFinder)
        return;

    if (m_directModuleFinder)
        m_directModuleFinder->disconnect(this);

    m_directModuleFinder = directModuleFinder;

    if (m_directModuleFinder)
        connect(m_directModuleFinder, &QnDirectModuleFinder::moduleFound, this, &QnWorkbenchServerAddressWatcher::at_directModuleFinder_moduleFound);
}

void QnWorkbenchServerAddressWatcher::setDirectModuleFinderHelper(QnDirectModuleFinderHelper *directModuleFinderHelper) {
    m_directModuleFinderHelper = directModuleFinderHelper;
}

void QnWorkbenchServerAddressWatcher::at_directModuleFinder_moduleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress, const QUrl &url) {
    Q_UNUSED(remoteAddress)
    Q_UNUSED(moduleInformation)

    if (m_urls.contains(url))
        return;

    m_urls.insert(url);
    qnSettings->setKnownServerUrls(m_urls.toList());
    if (m_directModuleFinderHelper)
        m_directModuleFinderHelper->setUrlsForPeriodicalCheck(m_urls);
}
