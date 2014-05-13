/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef NETWORKOPTIXMODULEFINDER_H
#define NETWORKOPTIXMODULEFINDER_H

#include <QtCore/QSet>
#include <QtCore/QHash>
#include <QtNetwork/QHostAddress>

#include <utils/common/long_runnable.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

#include "networkoptixmodulerevealcommon.h"
#include "aio/pollset.h"
#include "system_socket.h"

class AbstractDatagramSocket;

struct QnModuleInformation {
    QString type;
    QnSoftwareVersion version;
    QnSystemInformation systemInformation;
    QString systemName;
    TypeSpecificParamMap parameters;
    QSet<QString> remoteAddresses;
    bool isLocal; //!< true if at least one address from \a remoteHostAddress is a local address
    QString id;

    QnModuleInformation() : isLocal(false) {}
};

//!Searches for all Network Optix enterprise controllers in local network environment using multicast
/*!
    Search is done by sending multicast packet to predefined multicast group and waiting for an answer.
    Requests are sent periodically every \a pingTimeoutMillis milliseconds.
    If there was no response from previously found host for \a pingTimeoutMillis * \a keepAliveMultiply, than host is considered to be lost

    \note Requests are sent via all available local network interfaces
*/
class QnModuleFinder : public QnLongRunnable {
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
    QnModuleFinder(
        bool clientOnly,
        const QHostAddress &multicastGroupAddress = defaultModuleRevealMulticastGroup,
        const unsigned int multicastGroupPort = defaultModuleRevealMulticastGroupPort,
        const unsigned int pingTimeoutMillis = 0,
        const unsigned int keepAliveMultiply = 0);

    virtual ~QnModuleFinder();

    //! \returns true, if object has been successfully initialized (socket is created and binded to local address)
    bool isValid() const;

    /**
     * \returns                         Whether this module finder is working in compatibility mode.
     *                                  In this mode all EC are supported regardless of customization.
     */
    bool isCompatibilityMode() const;

    //! \param compatibilityMode         New compatibility mode state.
    void setCompatibilityMode(bool compatibilityMode);

    QList<QnModuleInformation> revealedModules() const;

public slots:
    virtual void pleaseStop() override;

signals:
    //!Emitted when new enterprise controller has been found
    void moduleFound(const QnModuleInformation &moduleInformation,
                     const QString &remoteAddress,
                     const QString &localInterfaceAddress);

    //!Emmited when previously found module did not respond to request in predefined timeout
    void moduleLost(const QnModuleInformation &moduleInformation);

protected:
    virtual void run() override;

private:
    bool processDiscoveryRequest(AbstractDatagramSocket *udpSocket);
    bool processDiscoveryResponse(AbstractDatagramSocket *udpSocket);

private:
    struct ModuleContext {
        QHostAddress address;
        RevealResponse response;
        quint64 prevResponseReceiveClock;
        QSet<QString> signalledAddresses;
        QnModuleInformation moduleInformation;

        ModuleContext(const RevealResponse &response);
    };

    PollSet m_pollSet;
    QList<AbstractDatagramSocket*> m_clientSockets;
    UDPSocket *m_serverSocket;
    const unsigned int m_pingTimeoutMillis;
    const unsigned int m_keepAliveMultiply;
    quint64 m_prevPingClock;
    QHash<QString, ModuleContext> m_knownEnterpriseControllers;
    QSet<QString> m_localNetworkAdresses;
    bool m_compatibilityMode;
};

Q_DECLARE_METATYPE(QnModuleInformation)

#endif  //NETWORKOPTIXMODULEFINDER_H
