#ifndef MULTICAST_MODULE_FINDER_H
#define MULTICAST_MODULE_FINDER_H

#include <QtCore/QMutex>
#include <QCache>

#include <utils/common/long_runnable.h>
#include "aio/pollset.h"
#include "networkoptixmodulerevealcommon.h"

class UDPSocket;
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

public slots:
    virtual void pleaseStop() override;
private slots:
    void at_moduleInformationChanged();
signals:
    void responseReceived(const QnModuleInformation &moduleInformation, const SocketAddress &address);

protected:
    virtual void run() override;

private:
    bool processDiscoveryRequest(UDPSocket *udpSocket);
    bool processDiscoveryResponse(UDPSocket *udpSocket);
    void updateInterfaces();
    RevealResponse* getCachedValue(const quint8* buffer, const quint8* bufferEnd);
private:
    mutable QMutex m_mutex;
    aio::PollSet m_pollSet;
    QHash<QHostAddress, UDPSocket*> m_clientSockets;
    UDPSocket *m_serverSocket;
    const unsigned int m_pingTimeoutMillis;
    const unsigned int m_keepAliveMultiply;
    quint64 m_prevPingClock;
    quint64 m_lastInterfacesCheckMs;
    bool m_compatibilityMode;
    QHostAddress m_multicastGroupAddress;
    quint16 m_multicastGroupPort;
    QCache<QByteArray, RevealResponse> m_cachedResponse;
    QByteArray m_serializedModuleInfo;
    mutable QMutex m_moduleInfoMutex;
};

#endif // MULTICAST_MODULE_FINDER_H
