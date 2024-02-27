#pragma once

#include <functional>
#include <mutex>
#include <thread>

#include <nx/network/socket_common.h>
#include <nx/network/system_socket.h>

class UdpProxy
{
public:
    // If callback returns false, packet will not be sent.
    using PacketCallback = std::function<bool(char* buf, std::size_t len)>;

    UdpProxy();
    ~UdpProxy();

    bool start(const nx::network::SocketAddress& listenAddr = nx::network::SocketAddress::anyPrivateAddressV4);

    nx::network::SocketAddress getProxyAddress() const;

    void addProxyRoute(
        const nx::network::SocketAddress& from,
        const nx::network::SocketAddress& to);

    void setBeforeSendingPacketCallback(PacketCallback callback);

private:
    void proxyThreadMain();

private:
    std::thread m_proxyThread;
    nx::network::UDPSocket m_socket;
    std::mutex m_mtx;
    std::map<nx::network::SocketAddress /*from*/, nx::network::SocketAddress /*to*/> m_rules;
    bool m_stopped = false;
    PacketCallback m_callback;
};
