#ifndef NETWORKOPTIXMODULEFINDER_H
#define NETWORKOPTIXMODULEFINDER_H

#include <QtCore/QHash>
#include <QtCore/QElapsedTimer>
#include <QtNetwork/QHostAddress>

#include <utils/common/singleton.h>
#include <utils/network/module_information.h>
#include <utils/network/socket_common.h>
#include <core/resource/resource_fwd.h>

class QnMulticastModuleFinder;
class QnDirectModuleFinder;
class QnDirectModuleFinderHelper;
class QTimer;

class QnModuleFinder : public QObject, public Singleton<QnModuleFinder> {
    Q_OBJECT
public:
    QnModuleFinder(bool clientMode, bool compatibilityMode);

    virtual ~QnModuleFinder();

    bool isCompatibilityMode() const;

    QList<QnModuleInformation> foundModules() const;
    QList<QnModuleInformationWithAddresses> foundModulesWithAddresses() const;

    QnModuleInformation moduleInformation(const QnUuid &moduleId) const;
    QSet<SocketAddress> moduleAddresses(const QnUuid &id) const;

    /** Get the best address to access the server with the given id.
      * This address could be NULL if server is about to be removed
      * or if all its interfaces was disabled by user.
      * Primary address for the current EC is always not NULL.
      * If user disabled all EC's interfaces, address used to connect
      * to EC will be used as primary address.
      */
    SocketAddress primaryAddress(const QnUuid &id) const;

    QnMulticastModuleFinder *multicastModuleFinder() const;
    QnDirectModuleFinder *directModuleFinder() const;
    QnDirectModuleFinderHelper *directModuleFinderHelper() const;

    int pingTimeout() const;

public slots:
    void start();
    void pleaseStop();

signals:
    void moduleChanged(const QnModuleInformation &moduleInformation);
    void moduleLost(const QnModuleInformation &moduleInformation);
    void moduleAddressFound(const QnModuleInformation &moduleInformation, const SocketAddress &address);
    void moduleAddressLost(const QnModuleInformation &moduleInformation, const SocketAddress &address);
    void moduleConflict(const QnModuleInformation &moduleInformation, const SocketAddress &address);

private:
    void at_responseReceived(const QnModuleInformation &moduleInformation, const SocketAddress &address);
    void at_timer_timeout();
    void at_server_auxUrlsChanged(const QnResourcePtr &resource);

    void removeAddress(const SocketAddress &address, bool holdItem, const QSet<QUrl> &ignoredUrls = QSet<QUrl>());
    void handleSelfResponse(const QnModuleInformation &moduleInformation, const SocketAddress &address);
    void sendModuleInformation(const QnModuleInformation &moduleInformation, const SocketAddress &address, bool isAlive);

private:
    /* Mutex prevents concurrent reading/wrighting of m_moduleItemById content.
     * All modifications are done in private methods and work always in the same thread,
     * so no need to preserve concurrent wrighting. Mutex should only guarantee
     * correct state of m_moduleItemById for public getters.
     */
    mutable QMutex m_itemsMutex;

    QElapsedTimer m_elapsedTimer;
    QTimer *m_timer;

    bool m_clientMode;

    QScopedPointer<QnMulticastModuleFinder> m_multicastModuleFinder;
    QnDirectModuleFinder *m_directModuleFinder;
    QnDirectModuleFinderHelper *m_helper;

    struct ModuleItem {
        QnModuleInformation moduleInformation;
        qint64 lastResponse;
        qint64 lastConflictResponse;
        int conflictResponseCount;
        QSet<SocketAddress> addresses;
        SocketAddress primaryAddress;

        ModuleItem() :
            lastResponse(0),
            lastConflictResponse(0),
            conflictResponseCount(0)
        {}
    };

    QHash<QnUuid, ModuleItem> m_moduleItemById;
    QHash<SocketAddress, QnUuid> m_idByAddress;
    QHash<SocketAddress, qint64> m_lastResponse;

    qint64 m_lastSelfConflict;
    int m_selfConflictCount;
};

#endif  //NETWORKOPTIXMODULEFINDER_H
