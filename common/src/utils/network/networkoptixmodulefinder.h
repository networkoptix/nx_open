/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef NETWORKOPTIXMODULEFINDER_H
#define NETWORKOPTIXMODULEFINDER_H

#include <set>
#include <vector>

#include <QHostAddress>
#include <QThread>

#include "networkoptixmodulerevealcommon.h"
#include "aio/pollset.h"
#include "../common/long_runnable.h"


class UDPSocket;

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
        One must call \a isValid after object instanciation to check wthether it has been initialized successfully

        \param multicastGroupAddress
        \param multicastGroupPort
        \param pingTimeoutMillis multicast group ping time. if 0, default value is used
        \param keepAliveMultiply if 0, default value is used
    */
    NetworkOptixModuleFinder(
        const QHostAddress& multicastGroupAddress = defaultModuleRevealMulticastGroup,
        const unsigned int multicastGroupPort = defaultModuleRevealMulticastGroupPort,
        const unsigned int pingTimeoutMillis = defaultPingTimeoutMillis,
        const unsigned int keepAliveMultiply = defaultKeepAliveMultiply );
    virtual ~NetworkOptixModuleFinder();

    //!Returns true, if object has been succesfully initialized (socket is created and binded to local address)
    bool isValid() const;

public slots:
    virtual void pleaseStop() override;

signals:
    //!Emmited when new enterprise controller has been found
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
    std::vector<UDPSocket*> m_sockets;
    const unsigned int m_pingTimeoutMillis;
    const unsigned int m_keepAliveMultiply;
    quint64 m_prevPingClock;
    //!map<seed, module data>
    std::map<QString, ModuleContext> m_knownEnterpriseControllers;
    std::set<QString> m_localNetworkAdresses;
};

#endif  //NETWORKOPTIXMODULEFINDER_H
