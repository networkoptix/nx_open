#include "workbench_server_address_watcher.h"

#include "utils/network/module_finder.h"
#include "utils/network/direct_module_finder.h"
#include "utils/network/module_information.h"
#include "client/client_settings.h"

namespace {
    const int checkInterval = 30 * 1000;
}

QnWorkbenchServerAddressWatcher::QnWorkbenchServerAddressWatcher(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    m_urls = QSet<QUrl>::fromList(qnSettings->knownServerUrls());

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &QnWorkbenchServerAddressWatcher::at_timer_timeout);
    timer->start(checkInterval);

    connect(QnModuleFinder::instance(), &QnModuleFinder::moduleUrlFound, this, &QnWorkbenchServerAddressWatcher::at_moduleFinder_moduleUrlFound);
    connect(QnModuleFinder::instance(), &QnModuleFinder::moduleUrlLost,  this, &QnWorkbenchServerAddressWatcher::at_moduleFinder_moduleUrlLost);

    QTimer::singleShot(1000, this, SLOT(at_timer_timeout()));
}

void QnWorkbenchServerAddressWatcher::at_moduleFinder_moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url) {
    Q_UNUSED(moduleInformation)

    m_onlineUrls.insert(url);

    if (m_urls.contains(url))
        return;

    m_urls.insert(url);
    qnSettings->setKnownServerUrls(m_urls.toList());
}

void QnWorkbenchServerAddressWatcher::at_moduleFinder_moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url) {
    Q_UNUSED(moduleInformation)
    m_onlineUrls.remove(url);
}

void QnWorkbenchServerAddressWatcher::at_timer_timeout() {
    QnDirectModuleFinder *directModuleFinder = QnModuleFinder::instance()->directModuleFinder();
    for (const QUrl &url: m_urls - m_onlineUrls)
        directModuleFinder->checkUrl(url);
}
