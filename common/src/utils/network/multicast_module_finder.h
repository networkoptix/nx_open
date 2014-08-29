#ifndef MULTICAST_MODULE_FINDER_H
#define MULTICAST_MODULE_FINDER_H

#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtNetwork/QHostAddress>

#include <utils/common/long_runnable.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

#include "module_information.h"
#include "network_address.h"
#include "networkoptixmodulerevealcommon.h"
#include "aio/pollset.h"
#include "system_socket.h"

class UDPSocket;

//!Searches for all Network Optix enterprise controllers in local network environment using multicast
/*!
    Search is done by sending multicast packet to predefined multicast group and waiting for an answer.
    Requests are sent periodically every \a pingTimeoutMillis milliseconds.
    If there was no response from previously found host for \a pingTimeoutMillis * \a keepAliveMultiply, than host is considered to be lost

    \note Requests are sent via all available local network interfaces
*/
class QnMulticastModuleFinder : public QnLongRunnable {
    Q_OBJECT
public:
    //!Creates socket and binds it to random unused udp port
    /*!
        One must call \a isValid after object instantiation to check whether it has been initialized successfully

        \param multicastGroupAddress
        \param multicastGroupPort
        \param pingTimeoutMillis multicast group ping time. if 0, default value is used
        \param keepAliveMultiply if 0, default value is used
    */
    QnMulticastModuleFinder(
        bool clientOnly,
        const QHostAddress &multicastGroupAddress = defaultModuleRevealMulticastGroup,
        const unsigned int multicastGroupPort = defaultModuleRevealMulticastGroupPort,
        const unsigned int pingTimeoutMillis = 0,
        const unsigned int keepAliveMultiply = 0);

    virtual ~QnMulticastModuleFinder();

    //! \returns true, if object has been successfully initialized (socket is created and binded to local address)
    bool isValid() const;

    /**
     * \returns                         Whether this module finder is working in compatibility mode.
     *                                  In this mode all EC are supported regardless of customization.
     */
    bool isCompatibilityMode() const;

    //! \param compatibilityMode         New compatibility mode state.
    void setCompatibilityMode(bool compatibilityMode);

    QnModuleInformation moduleInformation(const QUuid &moduleId) const;
    QList<QnModuleInformation> foundModules() const;

    void addIgnoredModule(const QnNetworkAddress &address, const QUuid &id);
    void removeIgnoredModule(const QnNetworkAddress &address, const QUuid &id);
    QMultiHash<QnNetworkAddress, QUuid> ignoredModules() const;

public slots:
    virtual void pleaseStop() override;

signals:
    void moduleChanged(const QnModuleInformation &moduleInformation);
    void moduleAddressFound(const QnModuleInformation &moduleInformation, const QnNetworkAddress &address);
    void moduleAddressLost(const QnModuleInformation &moduleInformation, const QnNetworkAddress &address);

protected:
    virtual void run() override;

private:
    bool processDiscoveryRequest(UDPSocket *udpSocket);
    bool processDiscoveryResponse(UDPSocket *udpSocket);

private:
    struct ModuleContext {
        quint64 prevResponseReceiveClock;
        QUuid moduleId;

        ModuleContext(const RevealResponse &response);
    };

    mutable QMutex m_mutex;
    aio::PollSet m_pollSet;
    QList<UDPSocket*> m_clientSockets;
    UDPSocket *m_serverSocket;
    const unsigned int m_pingTimeoutMillis;
    const unsigned int m_keepAliveMultiply;
    quint64 m_prevPingClock;
    bool m_compatibilityMode;
    QHash<QnNetworkAddress, ModuleContext> m_foundAddresses;
    QHash<QUuid, QnModuleInformation> m_foundModules;
    QMultiHash<QnNetworkAddress, QUuid> m_ignoredModules;
};

#endif // MULTICAST_MODULE_FINDER_H
