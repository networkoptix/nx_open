#ifndef NETWORKOPTIXMODULEFINDER_H
#define NETWORKOPTIXMODULEFINDER_H

#include <memory>

#include <QtCore/QHash>
#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>
#include <utils/common/singleton.h>
#include <utils/network/module_information.h>
#include <utils/network/network_address.h>

class QnMulticastModuleFinder;
class QnDirectModuleFinder;
class QnModuleFinderHelper;

class QnModuleFinder : public QObject, public Singleton<QnModuleFinder> {
    Q_OBJECT
public:
    QnModuleFinder(bool clientOnly);

    virtual ~QnModuleFinder();

    void setCompatibilityMode(bool compatibilityMode);
    bool isCompatibilityMode() const;

    QList<QnModuleInformation> foundModules() const;

    QnModuleInformation moduleInformation(const QnUuid &moduleId) const;

    QnMulticastModuleFinder *multicastModuleFinder() const;
    QnDirectModuleFinder *directModuleFinder() const;
    QnModuleFinderHelper *directModuleFinderHelper() const;

    //! \param peerList Discovery peer if and only if peer exist in peerList
    void setAllowedPeers(const QList<QnUuid> &peerList);

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

private:
    std::unique_ptr<QnMulticastModuleFinder> m_multicastModuleFinder;
    QnDirectModuleFinder *m_directModuleFinder;
    QnModuleFinderHelper *m_directModuleFinderHelper;

    QHash<QnUuid, QnModuleInformation> m_foundModules;
    QMultiHash<QnUuid, QUrl> m_multicastFoundUrls;
    QMultiHash<QnUuid, QUrl> m_directFoundUrls;
};

#endif  //NETWORKOPTIXMODULEFINDER_H
