#include "module_finder.h"

#include "multicast_module_finder.h"
#include "direct_module_finder.h"
#include "direct_module_finder_helper.h"

QnModuleFinder::QnModuleFinder(bool clientOnly) :
    m_multicastModuleFinder(new QnMulticastModuleFinder(clientOnly)),
    m_directModuleFinder(0),
    m_directModuleFinderHelper(0)
{
    connect(m_multicastModuleFinder,        &QnMulticastModuleFinder::moduleFound,      this,       &QnModuleFinder::at_moduleFound);
    connect(m_multicastModuleFinder,        &QnMulticastModuleFinder::moduleLost,       this,       &QnModuleFinder::at_moduleLost);
    if (!clientOnly) {
        m_directModuleFinder = new QnDirectModuleFinder(this);
        m_directModuleFinderHelper = new QnDirectModuleFinderHelper(this);
        connect(m_directModuleFinder,       &QnDirectModuleFinder::moduleFound,         this,       &QnModuleFinder::at_moduleFound);
        connect(m_directModuleFinder,       &QnDirectModuleFinder::moduleLost,          this,       &QnModuleFinder::at_moduleLost);
    }
}

QnModuleFinder::~QnModuleFinder() {
    stop();
}

void QnModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_multicastModuleFinder->setCompatibilityMode(compatibilityMode);
    if (m_directModuleFinder)
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

void QnModuleFinder::makeModulesReappear() {
    foreach (const QnModuleInformation &moduleInformation, m_foundModules) {
        emit moduleLost(moduleInformation);
        foreach (const QString &address, moduleInformation.remoteAddresses)
            emit moduleFound(moduleInformation, address);
    }
}

void QnModuleFinder::start() {
    m_multicastModuleFinder->start();
    if (m_directModuleFinder) {
        m_directModuleFinder->start();
        m_directModuleFinderHelper->setDirectModuleFinder(m_directModuleFinder);
    }
}

void QnModuleFinder::stop() {
    m_multicastModuleFinder->pleaseStop();
    if (m_directModuleFinder) {
        m_directModuleFinder->stop();
        m_directModuleFinderHelper->setDirectModuleFinder(NULL);
    }
}

void QnModuleFinder::at_moduleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress) {
    if (sender() == m_multicastModuleFinder && m_directModuleFinder)
        m_directModuleFinder->addIgnoredAddress(QHostAddress(remoteAddress), moduleInformation.port);

    auto it = m_foundModules.find(moduleInformation.id);
    if (it == m_foundModules.end()) {
        m_foundModules.insert(moduleInformation.id, moduleInformation);
    } else {
        QSet<QString> oldAddresses = it->remoteAddresses;
        if (oldAddresses.contains(remoteAddress)) {
            if (moduleInformation.systemName == it->systemName)
                return;
        }

        // update module information collecting all known addresses
        *it = moduleInformation;
        it->remoteAddresses += oldAddresses;
    }

    emit moduleFound(moduleInformation, remoteAddress);
}

void QnModuleFinder::at_moduleLost(const QnModuleInformation &moduleInformation) {
    bool stay = true;

    if (sender() == m_multicastModuleFinder) {
        if (m_directModuleFinder) {
            foreach (const QString &address, moduleInformation.remoteAddresses)
                m_directModuleFinder->removeIgnoredAddress(QHostAddress(address), moduleInformation.port);

            if (m_directModuleFinder->moduleInformation(moduleInformation.id).id.isNull())
                stay = false;
        } else {
            stay = false;
        }
    } else {
        if (m_multicastModuleFinder->moduleInformation(moduleInformation.id.toString()).id.isNull())
            stay = false;
    }

    if (!stay) {
        m_foundModules.remove(moduleInformation.id);
        emit moduleLost(moduleInformation);
    }
}
