#ifndef MULTICAST_MODULE_FINDER_H
#define MULTICAST_MODULE_FINDER_H

#include <memory>

#include <QCache>

#include <utils/common/long_runnable.h>
#include <nx/network/aio/pollset.h>
#include <nx/utils/thread/mutex.h>

#include "networkoptixmodulerevealcommon.h"


namespace nx {
namespace network {

class UDPSocket;

}   //network
}   //nx

struct QnModuleInformation;
class SocketAddress;

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
        const QHostAddress &multicastGroupAddress = QHostAddress(),
        const quint16 multicastGroupPort = 0,
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

    void setCheckInterfacesTimeout(unsigned int checkInterfacesTimeoutMs);

    // TODO: #mux Fix the comment.
    //! Returns \fn run (DEBUG ONLY!)
    static bool isDisabled;

public slots:
    virtual void pleaseStop() override;

private slots:
    void at_moduleInformationChanged();
signals:
    void responseReceived(const QnModuleInformation &moduleInformation, const SocketAddress &address);

protected:
    virtual void run() override;

private:
    bool processDiscoveryRequest(nx::network::UDPSocket *udpSocket);
    bool processDiscoveryResponse(nx::network::UDPSocket *udpSocket);
    void updateInterfaces();
    void clearInterfaces();
    RevealResponse* getCachedValue(const quint8* buffer, const quint8* bufferEnd);

private:
    mutable QnMutex m_mutex;
    bool m_clientMode;
    nx::network::aio::PollSet m_pollSet;
    QHash<QHostAddress, nx::network::UDPSocket*> m_clientSockets;
    std::unique_ptr<nx::network::UDPSocket> m_serverSocket;
    const unsigned int m_pingTimeoutMillis;
    const unsigned int m_keepAliveMultiply;
    quint64 m_prevPingClock;
    unsigned int m_checkInterfacesTimeoutMs;
    quint64 m_lastInterfacesCheckMs;
    bool m_compatibilityMode;
    QHostAddress m_multicastGroupAddress;
    quint16 m_multicastGroupPort;
    QCache<QByteArray, RevealResponse> m_cachedResponse;
    QByteArray m_serializedModuleInfo;
    mutable QnMutex m_moduleInfoMutex;
};

#endif // MULTICAST_MODULE_FINDER_H
