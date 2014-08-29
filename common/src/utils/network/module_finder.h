#ifndef NETWORKOPTIXMODULEFINDER_H
#define NETWORKOPTIXMODULEFINDER_H

#include <QtCore/QHash>
#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>
#include <utils/common/singleton.h>
#include <utils/network/module_information.h>
#include <utils/network/full_network_address.h>

class QnMulticastModuleFinder;
class QnDirectModuleFinder;
class QnModuleFinderHelper;

class QnModuleFinder : public QObject, public Singleton<QnModuleFinder> {
    Q_OBJECT
public:
    QnModuleFinder(bool clientOnly);

    virtual ~QnModuleFinder();

    void setCompatibilityMode(bool compatibilityMode);

    QList<QnModuleInformation> foundModules() const;

    QnModuleInformation moduleInformation(const QString &moduleId) const;

    QnMulticastModuleFinder *multicastModuleFinder() const;
    QnDirectModuleFinder *directModuleFinder() const;
    QnModuleFinderHelper *directModuleFinderHelper() const;

    //! \param peerList Discovery peer if and only if peer exist in peerList
    void setAllowedPeers(const QList<QUuid> &peerList);

public slots:
    void start();
    void stop();
    void pleaseStop();
signals:
    void moduleChanged(const QnModuleInformation &moduleInformation);
    void moduleLost(const QnModuleInformation &moduleInformation);
    void moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url);
    void moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url);

private slots:
    void at_moduleAddressFound(const QnModuleInformation &moduleInformation, const QnNetworkAddress &address);
    void at_moduleAddressLost(const QnModuleInformation &moduleInformation, const QnNetworkAddress &address);
    void at_moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url);
    void at_moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url);
    void at_moduleChanged(const QnModuleInformation &moduleInformation);
    void at_resourcePool_resourceChanged(const QnResourcePtr &resource);

private:
    QnMulticastModuleFinder *m_multicastModuleFinder;
    QnDirectModuleFinder *m_directModuleFinder;
    QnModuleFinderHelper *m_directModuleFinderHelper;

    QHash<QUuid, QnModuleInformation> m_foundModules;
    QMultiHash<QUuid, QUrl> m_multicastFoundUrls;
    QMultiHash<QUuid, QUrl> m_directFoundUrls;
    QList<QUuid> m_allowedPeers;
};

#endif  //NETWORKOPTIXMODULEFINDER_H
