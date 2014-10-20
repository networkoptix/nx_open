#include "module_finder.h"

#include "utils/common/log.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "multicast_module_finder.h"
#include "direct_module_finder.h"
#include "direct_module_finder_helper.h"

namespace {
    QUrl trimmedUrl(const QUrl &url) {
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
    m_directModuleFinderHelper(new QnModuleFinderHelper(this))
{
    connect(m_multicastModuleFinder,        &QnMulticastModuleFinder::moduleAddressFound,       this,       &QnModuleFinder::at_moduleAddressFound);
    connect(m_multicastModuleFinder,        &QnMulticastModuleFinder::moduleAddressLost,        this,       &QnModuleFinder::at_moduleAddressLost);
    connect(m_multicastModuleFinder,        &QnMulticastModuleFinder::moduleChanged,            this,       &QnModuleFinder::at_moduleChanged);
    connect(m_directModuleFinder,           &QnDirectModuleFinder::moduleUrlFound,              this,       &QnModuleFinder::at_moduleUrlFound);
    connect(m_directModuleFinder,           &QnDirectModuleFinder::moduleUrlLost,               this,       &QnModuleFinder::at_moduleUrlLost);
    connect(m_directModuleFinder,           &QnDirectModuleFinder::moduleChanged,               this,       &QnModuleFinder::at_moduleChanged);
    //connect(qnResPool,                      &QnResourcePool::resourceAdded,                     this,       &QnModuleFinder::at_resourcePool_resourceChanged);
    //connect(qnResPool,                      &QnResourcePool::resourceChanged,                   this,       &QnModuleFinder::at_resourcePool_resourceChanged);
}

QnModuleFinder::~QnModuleFinder() {
    stop();
}

void QnModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_multicastModuleFinder->setCompatibilityMode(compatibilityMode);
    m_directModuleFinder->setCompatibilityMode(compatibilityMode);
}

bool QnModuleFinder::isCompatibilityMode() const
{
    return m_directModuleFinder->isCompatibilityMode();
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

void QnModuleFinder::setAllowedPeers(const QList<QnUuid> &peerList) {
    m_allowedPeers = peerList;
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

    QUrl url = address.toUrl();
    m_directModuleFinder->addIgnoredUrl(url);

    if (!m_multicastFoundUrls.contains(moduleInformation.id, url)) {
        m_multicastFoundUrls.insert(moduleInformation.id, url);

        if (!m_directFoundUrls.contains(moduleInformation.id, url)) {
            NX_LOG(lit("QnModuleFinder: New URL from multicast finder: %1 %2").arg(moduleInformation.id.toString()).arg(url.toString()), cl_logDEBUG1);
            emit moduleUrlFound(m_foundModules.value(moduleInformation.id), url);
        }
    }
}

void QnModuleFinder::at_moduleAddressLost(const QnModuleInformation &moduleInformation, const QnNetworkAddress &address) {
    if (!m_allowedPeers.isEmpty() && !m_allowedPeers.contains(moduleInformation.id))
        return;

    QUrl url = address.toUrl();

    m_directModuleFinder->removeIgnoredUrl(url);
    if (m_multicastFoundUrls.remove(moduleInformation.id, url)) {
        NX_LOG(lit("QnModuleFinder: URL from multicast finder is lost: %1 %2").arg(moduleInformation.id.toString()).arg(url.toString()), cl_logDEBUG1);
        emit moduleUrlLost(m_foundModules.value(moduleInformation.id), url);

        QnModuleInformation locModuleInformation = m_foundModules.value(moduleInformation.id);
        if (locModuleInformation.remoteAddresses.isEmpty()) {
            m_foundModules.remove(moduleInformation.id);
            NX_LOG(lit("QnModuleFinder: Module %1 lost.").arg(moduleInformation.id.toString()), cl_logDEBUG1);
            emit moduleLost(locModuleInformation);
        }
    }
}

void QnModuleFinder::at_moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &foundUrl) {
    if (!m_allowedPeers.isEmpty() && !m_allowedPeers.contains(moduleInformation.id))
        return;

    QUrl url = trimmedUrl(foundUrl);

    if (!m_directFoundUrls.contains(moduleInformation.id, url)) {
        m_directFoundUrls.insert(moduleInformation.id, url);

        if (!m_multicastFoundUrls.contains(moduleInformation.id, url)) {
            NX_LOG(lit("QnModuleFinder: New URL from direct finder: %1 %2").arg(moduleInformation.id.toString()).arg(url.toString()), cl_logDEBUG1);
            emit moduleUrlFound(m_foundModules.value(moduleInformation.id), url);
        }
    }
}

void QnModuleFinder::at_moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &foundUrl) {
    if (!m_allowedPeers.isEmpty() && !m_allowedPeers.contains(moduleInformation.id))
        return;

    QUrl url = trimmedUrl(foundUrl);

    if (m_directFoundUrls.remove(moduleInformation.id, url)) {
        NX_LOG(lit("QnModuleFinder: URL from direct finder is lost: %1 %2").arg(moduleInformation.id.toString()).arg(url.toString()), cl_logDEBUG1);
        emit moduleUrlLost(m_foundModules.value(moduleInformation.id), url);

        QnModuleInformation locModuleInformation = m_foundModules.value(moduleInformation.id);
        if (locModuleInformation.remoteAddresses.isEmpty()) {
            m_foundModules.remove(moduleInformation.id);
            NX_LOG(lit("QnModuleFinder: Module %1 lost.").arg(moduleInformation.id.toString()), cl_logDEBUG1);
            emit moduleLost(locModuleInformation);
        }
    }
}

void QnModuleFinder::at_moduleChanged(const QnModuleInformation &moduleInformation) {
    if (!m_allowedPeers.isEmpty() && !m_allowedPeers.contains(moduleInformation.id))
        return;

    QnModuleInformation updatedModuleInformation = moduleInformation;

    if (sender() == m_multicastModuleFinder)
        updatedModuleInformation.remoteAddresses.unite(m_directModuleFinder->moduleInformation(moduleInformation.id).remoteAddresses);
    else if (sender() == m_directModuleFinder)
        updatedModuleInformation.remoteAddresses.unite(m_multicastModuleFinder->moduleInformation(moduleInformation.id).remoteAddresses);
    else
        Q_ASSERT_X(0, "Invalid sender in slot", Q_FUNC_INFO);

    QnModuleInformation &oldModuleInformation = m_foundModules[moduleInformation.id];
    if (oldModuleInformation != updatedModuleInformation) {
        oldModuleInformation = updatedModuleInformation;

        /* Don't emit when there is no more remote addresses. moduleLost() will be emited just after moduleUrlLost(). */
        if (!updatedModuleInformation.remoteAddresses.isEmpty()) {
            NX_LOG(lit("QnModuleFinder. Module %1 is changed, addresses = [%2]")
                   .arg(updatedModuleInformation.id.toString())
                   .arg(QStringList(QStringList::fromSet(updatedModuleInformation.remoteAddresses)).join(lit(", "))), cl_logDEBUG1);
            emit moduleChanged(updatedModuleInformation);
        }
    }
}

/*
void QnModuleFinder::at_resourcePool_resourceChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    auto it = m_foundModules.find(server->getId());
    if (it == m_foundModules.end())
        return;

    QnModuleInformation moduleInformation = server->getModuleInformation();
    moduleInformation.remoteAddresses = it->remoteAddresses;
    moduleInformation.sslAllowed = it->sslAllowed;

    if (moduleInformation == *it)
        return;

    *it = moduleInformation;
    emit moduleChanged(moduleInformation);
}
*/

void QnModuleFinder::stop() {
    m_multicastModuleFinder->stop();
    m_directModuleFinder->stop();
}

