// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <limits>

#include <QtCore/QCache>
#include <QtNetwork/QHostAddress>

#include <nx/network/aio/pollset.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/types/connection_types.h>

namespace nx::network {

class UDPSocket;
class HostAddress;
class SocketAddress;

} // namespace nx::network

namespace nx::vms::discovery {

/**
 * Searches for all compatible servers in local network environment using multicast.
 *
 * Search is done by sending multicast packet to predefined multicast group and waiting for an answer.
 * Requests are sent periodically every \a pingTimeoutMillis milliseconds.
 * If there was no response from previously found host for
 * \a pingTimeoutMillis * \a keepAliveMultiply,
 * than host is considered to be lost.
 *
 * Requests are sent via all available local network interfaces.
 */
class DeprecatedMulticastFinder: public QnLongRunnable
{
    Q_OBJECT

public:
    struct Options
    {
        bool listenAndRespond = false;
        size_t multicastCount = 0;
        std::function<bool()> responseEnabled;
        nx::Uuid peerId;

        static constexpr size_t kUnlimited = std::numeric_limits<size_t>::max();
    };

    //!Creates socket and binds it to random unused udp port
    /*!
        One must call \a isValid after object instantiation to check whether it has been initialized successfully

        \param multicastGroupAddress
        \param multicastGroupPort
        \param pingTimeoutMillis multicast group ping time. if 0, default value is used
        \param keepAliveMultiply if 0, default value is used
    */
    DeprecatedMulticastFinder(
        Options options,
        const QHostAddress &multicastGroupAddress = QHostAddress(),
        const quint16 multicastGroupPort = 0,
        const unsigned int pingTimeoutMillis = 0,
        const unsigned int keepAliveMultiply = 0);

    virtual ~DeprecatedMulticastFinder();

    //! \returns true, if object has been successfully initialized (socket is created and binded to local address)
    bool isValid() const;

    void setCheckInterfacesTimeout(unsigned int checkInterfacesTimeoutMs);

    void setMulticastInformation(const nx::vms::api::ModuleInformation& information);

    // TODO: #muskov Fix the comment.
    //! Returns \fn run (DEBUG ONLY!)
    static bool isDisabled;

public slots:
    virtual void pleaseStop() override;

signals:
    void responseReceived(
        const nx::vms::api::ModuleInformation& moduleInformation,
        const nx::network::SocketAddress& endpoint, const nx::network::HostAddress& ip);

protected:
    virtual void run() override;

private:
    class RevealRequest
    {
    public:
        RevealRequest(const nx::Uuid& moduleGuid, nx::vms::api::PeerType peerType);

        QByteArray serialize();
        static bool isValid(const quint8 *bufStart, const quint8 *bufEnd);
    private:
        const nx::Uuid m_moduleGuid;
        const nx::vms::api::PeerType m_peerType;
    };

    //!Sent in response to RevealRequest by module which reveals itself
    struct RevealResponse : public nx::vms::api::ModuleInformation {
    public:
        RevealResponse() {}
        RevealResponse(const nx::vms::api::ModuleInformation &other);
        QByteArray serialize();
        bool deserialize(const quint8 *bufStart, const quint8 *bufEnd);
    };

    bool processDiscoveryRequest(nx::network::UDPSocket *udpSocket);
    bool processDiscoveryResponse(nx::network::UDPSocket *udpSocket);
    void updateInterfaces();
    void clearInterfaces();
    RevealResponse* getCachedValue(const quint8* buffer, const quint8* bufferEnd);

private:
    mutable nx::Mutex m_mutex;
    Options m_options;
    nx::network::aio::PollSet m_pollSet;
    std::map<nx::network::HostAddress, std::unique_ptr<nx::network::UDPSocket>> m_clientSockets;
    std::unique_ptr<nx::network::UDPSocket> m_serverSocket;
    const unsigned int m_pingTimeoutMillis;
    const unsigned int m_keepAliveMultiply;
    quint64 m_prevPingClock;
    unsigned int m_checkInterfacesTimeoutMs;
    quint64 m_lastInterfacesCheckMs;
    QHostAddress m_multicastGroupAddress;
    quint16 m_multicastGroupPort;
    QCache<QByteArray, RevealResponse> m_cachedResponse;
    QByteArray m_serializedModuleInfo;
    mutable nx::Mutex m_moduleInfoMutex;
};

} // namespace nx::vms::discovery
