/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef NETWORKOPTIXMODULEFINDER_H
#define NETWORKOPTIXMODULEFINDER_H

#include <set>
#include <vector>

#include <QtNetwork/QHostAddress>

#include <utils/common/long_runnable.h>

#include "networkoptixmodulerevealcommon.h"
#include "aio/pollset.h"
#include "system_socket.h"

class AbstractDatagramSocket;

//!Searches for all Network Optix enterprise controllers in local network environment using multicast
/*!
    Search is done by sending multicast packet to predefined multicast group and waiting for an answer.
    Requests are sent periodically every \a pingTimeoutMillis milliseconds.
    If there was no response from previously found host for \a pingTimeoutMillis * \a keepAliveMultiply, than host is considered to be lost

    \note Requests are sent via all available local network interfaces
*/
class NetworkOptixModuleFinder
:
    public QnLongRunnable
{
    Q_OBJECT

public:
    static const unsigned int defaultPingTimeoutMillis = 3000;
    static const unsigned int defaultKeepAliveMultiply = 3;

    //!Creates socket and binds it to random unused udp port
    /*!
        One must call \a isValid after object instantiation to check whether it has been initialized successfully

        \param multicastGroupAddress
        \param multicastGroupPort
        \param pingTimeoutMillis multicast group ping time. if 0, default value is used
        \param keepAliveMultiply if 0, default value is used
    */
    NetworkOptixModuleFinder(
        bool clientOnly,
        const QHostAddress& multicastGroupAddress = defaultModuleRevealMulticastGroup,
        const unsigned int multicastGroupPort = defaultModuleRevealMulticastGroupPort,
        const unsigned int pingTimeoutMillis = defaultPingTimeoutMillis,
        const unsigned int keepAliveMultiply = defaultKeepAliveMultiply);
    virtual ~NetworkOptixModuleFinder();

    //!Returns true, if object has been successfully initialized (socket is created and binded to local address)
    bool isValid() const;

    /**
     * \returns                         Whether this module finder is working in compatibility mode.
     *                                  In this mode all EC are supported regardless of customization.
     */
    bool isCompatibilityMode() const;

    /**
     * \param compatibilityMode         New compatibility mode state.
     */
    void setCompatibilityMode(bool compatibilityMode);

public slots:
    virtual void pleaseStop() override;

signals:
    // TODO: #Elric deal with +inf number of args here

    //!Emitted when new enterprise controller has been found
    /*!
        \param moduleType type, reported by module
        \param moduleVersion
        \param moduleParameters
        \param localInterfaceIP IP address of local interface, response from enterprise controller has been received on
        \param remoteHostAddress Address of found enterprise controller
        \param isLocal true, if \a remoteHostAddress is a local address
        \param moduleSeed unique string, reported by module
    */
    void moduleFound(
        const QString& moduleType,
        const QString& moduleVersion,
        const QString& systemName,
        const TypeSpecificParamMap& moduleParameters,
        const QString& localInterfaceAddress,
        const QString& remoteHostAddress,
        bool isLocal,
        const QString& moduleSeed );
    //!Emmited when previously found module did not respond to request in predefined timeout
    /*!
        \param isLocal true, if \a remoteHostAddress is a local address
    */
    void moduleLost(
        const QString& moduleType,
        const TypeSpecificParamMap& moduleParameters,
        const QString& remoteHostAddress,
        bool isLocal,
        const QString& moduleSeed );

protected:
    virtual void run() override;
private:
    bool processDiscoveryRequest(AbstractDatagramSocket* udpSocket);
    bool processDiscoveryResponse(AbstractDatagramSocket* udpSocket);
private:
    class ModuleContext
    {
    public:
        QHostAddress address;
        RevealResponse response;
        quint64 prevResponseReceiveClock;
        std::set<QString> signalledAddresses;

        ModuleContext()
        :
            prevResponseReceiveClock( 0 )
        {
        }
    };

    PollSet m_pollSet;
    std::vector<AbstractDatagramSocket*> m_clientSockets;
    UDPSocket* m_serverSocket;
    const unsigned int m_pingTimeoutMillis;
    const unsigned int m_keepAliveMultiply;
    quint64 m_prevPingClock;
    //!map<seed, module data>
    std::map<QString, ModuleContext> m_knownEnterpriseControllers;
    std::set<QString> m_localNetworkAdresses;
    bool m_compatibilityMode;
};

#endif  //NETWORKOPTIXMODULEFINDER_H
