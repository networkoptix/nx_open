#include "module_finder.h"

#include "multicast_module_finder.h"
#include "direct_module_finder.h"
#include "direct_module_finder_helper.h"

namespace {
    QUrl addressToUrl(const QnNetworkAddress &address) {
        return QUrl(lit("http://%1:%2").arg(address.host().toString()).arg(address.port()));
    }

    QUrl trimUrl(const QUrl &url) {
        QUrl newUrl;
        newUrl.setScheme(lit("http"));
        newUrl.setHost(url.host());
        newUrl.setPort(url.port());
        return newUrl;
    }
}

QnModuleFinder::QnModuleFinder(bool clientOnly) :
    m_multicastModuleFinder(new QnMulticastModuleFinder(clientOnly)),
    m_directModuleFinder(new QnDirectModuleFinder(this)),
    m_directModuleFinderHelper(new QnModuleFinderHelper(this)),
    m_sendingFakeSignals(false)
{
    connect(m_multicastModuleFinder,        &QnMulticastModuleFinder::moduleAddressFound,       this,       &QnModuleFinder::at_moduleAddressFound);
    connect(m_multicastModuleFinder,        &QnMulticastModuleFinder::moduleAddressLost,        this,       &QnModuleFinder::at_moduleAddressLost);
    connect(m_multicastModuleFinder,        &QnMulticastModuleFinder::moduleChanged,            this,       &QnModuleFinder::at_moduleChanged);
    connect(m_directModuleFinder,           &QnDirectModuleFinder::moduleUrlFound,              this,       &QnModuleFinder::at_moduleUrlFound);
    connect(m_directModuleFinder,           &QnDirectModuleFinder::moduleUrlLost,               this,       &QnModuleFinder::at_moduleUrlLost);
    connect(m_directModuleFinder,           &QnDirectModuleFinder::moduleChanged,               this,       &QnModuleFinder::at_moduleChanged);
}

QnModuleFinder::~QnModuleFinder() {
    stop();
}

void QnModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_multicastModuleFinder->setCompatibilityMode(compatibilityMode);
    m_directModuleFinder->setCompatibilityMode(compatibilityMode);
}

QList<QnModuleInformation> QnModuleFinder::foundModules() const {
    return m_foundModules.values();
}

QnModuleInformation QnModuleFinder::moduleInformation(const QString &moduleId) const {
    return m_foundModules.value(moduleId);
}

QnMulticastModuleFinder *QnModuleFinder::multicastModuleFinder() const {
    return m_multicastModuleFinder;
}

QnDirectModuleFinder *QnModuleFinder::directModuleFinder() const {
    return m_directModuleFinder;
}

QnModuleFinderHelper *QnModuleFinder::directModuleFinderHelper() const {
    return m_directModuleFinderHelper;
}

void QnModuleFinder::makeModulesReappear() {
    m_sendingFakeSignals = true;
//    foreach (const QnModuleInformation &moduleInformation, m_foundModules) {
//        emit moduleLost(moduleInformation);
//        foreach (const QString &address, moduleInformation.remoteAddresses)
//            emit moduleFound(moduleInformation, address);
//    }
    m_sendingFakeSignals = false;
}

void QnModuleFinder::setAllowedPeers(const QList<QUuid> &peerList) {
    m_allowedPeers = peerList;
}

bool QnModuleFinder::isSendingFakeSignals() const {
    return m_sendingFakeSignals;
}

void QnModuleFinder::start() {
    m_multicastModuleFinder->start();
    m_directModuleFinder->start();
}

void QnModuleFinder::pleaseStop() {
    m_multicastModuleFinder->pleaseStop();
    m_directModuleFinder->pleaseStop();
}

void QnModuleFinder::at_moduleAddressFound(const QnModuleInformation &moduleInformation, const QnNetworkAddress &address) {
    if (!m_allowedPeers.isEmpty() && !m_allowedPeers.contains(moduleInformation.id))
        return;

    QnModuleInformation &oldModuleInformation = m_foundModules[moduleInformation.id];
    if (oldModuleInformation != moduleInformation) {
        oldModuleInformation = moduleInformation;
        emit moduleChanged(moduleInformation);
    }

    QUrl url = addressToUrl(address);

    m_directModuleFinder->addIgnoredAddress(address.host(), address.port());

    if (!m_multicastFoundUrls.contains(moduleInformation.id, url)) {
        m_multicastFoundUrls.insert(moduleInformation.id, url);

        if (!m_directFoundUrls.contains(moduleInformation.id, url))
            emit moduleUrlFound(moduleInformation, url);
    }
}

void QnModuleFinder::at_moduleAddressLost(const QnModuleInformation &moduleInformation, const QnNetworkAddress &address) {
    if (!m_allowedPeers.isEmpty() && !m_allowedPeers.contains(moduleInformation.id))
        return;

    QUrl url = addressToUrl(address);

    m_directModuleFinder->removeIgnoredAddress(address.host(), address.port());
    if (m_multicastFoundUrls.remove(moduleInformation.id, url)) {
        emit moduleUrlLost(m_foundModules.value(moduleInformation.id), url);

        if (!m_multicastFoundUrls.contains(moduleInformation.id) && !m_directFoundUrls.contains(moduleInformation.id))
            emit moduleLost(m_foundModules.take(moduleInformation.id));
    }
}

void QnModuleFinder::at_moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &foundUrl) {
    if (!m_allowedPeers.isEmpty() && !m_allowedPeers.contains(moduleInformation.id))
        return;

    QnModuleInformation &oldModuleInformation = m_foundModules[moduleInformation.id];
    if (oldModuleInformation != moduleInformation) {
        oldModuleInformation = moduleInformation;
        emit moduleChanged(moduleInformation);
    }

    QUrl url = trimUrl(foundUrl);

    if (!m_directFoundUrls.contains(moduleInformation.id, url)) {
        m_directFoundUrls.insert(moduleInformation.id, url);

        if (!m_multicastFoundUrls.contains(moduleInformation.id, url))
            emit moduleUrlFound(moduleInformation, url);
    }
}

void QnModuleFinder::at_moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &foundUrl) {
    if (!m_allowedPeers.isEmpty() && !m_allowedPeers.contains(moduleInformation.id))
        return;

    QUrl url = trimUrl(foundUrl);

    if (m_directFoundUrls.remove(moduleInformation.id, url)) {
        emit moduleUrlLost(m_foundModules.value(moduleInformation.id), url);

        if (!m_multicastFoundUrls.contains(moduleInformation.id) && !m_directFoundUrls.contains(moduleInformation.id))
            emit moduleLost(m_foundModules.take(moduleInformation.id));
    }

}

void QnModuleFinder::at_moduleChanged(const QnModuleInformation &moduleInformation) {
    if (!m_allowedPeers.isEmpty() && !m_allowedPeers.contains(moduleInformation.id))
        return;

    QnModuleInformation &oldModuleInformation = m_foundModules[moduleInformation.id];
    if (!oldModuleInformation.id.isNull() && oldModuleInformation != moduleInformation) {
        oldModuleInformation = moduleInformation;
        emit moduleChanged(moduleInformation);
    }
}

void QnModuleFinder::stop() {
    m_multicastModuleFinder->stop();
    m_directModuleFinder->stop();
}

