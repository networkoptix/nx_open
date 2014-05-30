#include "module_finder.h"

#include "multicast_module_finder.h"
#include "direct_module_finder.h"
#include "direct_module_finder_helper.h"

QnModuleFinder::QnModuleFinder(bool clientOnly) :
    m_multicastModuleFinder(new QnMulticastModuleFinder(clientOnly)),
    m_directModuleFinder(new QnDirectModuleFinder(this)),
    m_directModuleFinderHelper(new QnDirectModuleFinderHelper(this))
{
    connect(m_multicastModuleFinder,        &QnMulticastModuleFinder::moduleFound,      this,       &QnModuleFinder::at_moduleFound);
    connect(m_multicastModuleFinder,        &QnMulticastModuleFinder::moduleLost,       this,       &QnModuleFinder::at_moduleLost);
    connect(m_directModuleFinder,           &QnDirectModuleFinder::moduleFound,         this,       &QnModuleFinder::at_moduleFound);
    connect(m_directModuleFinder,           &QnDirectModuleFinder::moduleLost,          this,       &QnModuleFinder::at_moduleLost);
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

void QnModuleFinder::start() {
    m_multicastModuleFinder->start();
    m_directModuleFinder->start();
    m_directModuleFinderHelper->setDirectModuleFinder(m_directModuleFinder);
}

void QnModuleFinder::stop() {
    m_multicastModuleFinder->pleaseStop();
    m_directModuleFinder->stop();
    m_directModuleFinderHelper->setDirectModuleFinder(NULL);
}

void QnModuleFinder::at_moduleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress) {
    if (sender() == m_multicastModuleFinder)
        m_directModuleFinder->addIgnoredAddress(QHostAddress(remoteAddress), moduleInformation.port);

    auto it = m_foundModules.find(moduleInformation.id);
    if (it == m_foundModules.end()) {
        m_foundModules.insert(moduleInformation.id, moduleInformation);
    } else {
        QSet<QString> oldAddresses = it->remoteAddresses;
        if (oldAddresses.contains(remoteAddress))
            return;

        // update module information collecting all known addresses
        *it = moduleInformation;
        it->remoteAddresses += oldAddresses;
    }
}

void QnModuleFinder::at_moduleLost(const QnModuleInformation &moduleInformation) {
    bool stay = true;

    if (sender() == m_multicastModuleFinder) {
        foreach (const QString &address, moduleInformation.remoteAddresses)
            m_directModuleFinder->removeIgnoredAddress(QHostAddress(address), moduleInformation.port);

        if (m_directModuleFinder->moduleInformation(moduleInformation.id).id.isNull())
            stay = false;
    } else {
        if (m_multicastModuleFinder->moduleInformation(moduleInformation.id.toString()).id.isNull())
            stay = false;
    }

    if (!stay)
        emit moduleLost(moduleInformation);
}
