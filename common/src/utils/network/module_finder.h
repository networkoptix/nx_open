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
    QnModuleFinder(bool clientOnly);

    virtual ~QnModuleFinder();

    void setCompatibilityMode(bool compatibilityMode);
    bool isCompatibilityMode() const;

    QList<QnModuleInformation> foundModules() const;

    QnModuleInformation moduleInformation(const QnUuid &moduleId) const;
    QSet<QString> moduleAddresses(const QnUuid &id) const;

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

private slots:
    void at_responseReceived(const QnModuleInformation &moduleInformation, const QString &address);
    void at_timer_timeout();

private:
    void removeAddress(const SocketAddress &address, bool holdItem);
    void handleSelfResponse(const QnModuleInformation &moduleInformation, const QString &address);

private:
    QElapsedTimer m_elapsedTimer;
    QTimer *m_timer;

    QScopedPointer<QnMulticastModuleFinder> m_multicastModuleFinder;
    QnDirectModuleFinder *m_directModuleFinder;
    QnDirectModuleFinderHelper *m_helper;

    struct ModuleItem {
        QnModuleInformation moduleInformation;
        qint64 lastResponse;
        qint64 lastConflictResponse;
        int conflictResponseCount;
        QSet<QString> addresses;
    };

    QHash<QnUuid, ModuleItem> m_moduleItemById;
    QHash<SocketAddress, QnUuid> m_idByAddress;
    QHash<SocketAddress, qint64> m_lastResponse;

    qint64 m_lastSelfConflict;
    int m_selfConflictCount;
};

#endif  //NETWORKOPTIXMODULEFINDER_H
