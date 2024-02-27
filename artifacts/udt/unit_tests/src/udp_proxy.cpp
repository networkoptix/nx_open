#include "udp_proxy.h"

UdpProxy::UdpProxy()
{
}

UdpProxy::~UdpProxy()
{
    m_stopped = true;

    if (m_proxyThread.joinable())
        m_proxyThread.join();
}

bool UdpProxy::start(const nx::network::SocketAddress& listenAddr)
{
    if (!m_socket.bind(listenAddr))
        return false;
    m_socket.setRecvTimeout(500);

    m_proxyThread = std::thread([this]() { proxyThreadMain(); });
    return true;
}

nx::network::SocketAddress UdpProxy::getProxyAddress() const
{
    return m_socket.getLocalAddress();
}

void UdpProxy::addProxyRoute(
    const nx::network::SocketAddress& from,
    const nx::network::SocketAddress& to)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_rules.emplace(from, to);
}

void UdpProxy::setBeforeSendingPacketCallback(PacketCallback callback)
{
    m_callback = std::move(callback);
}

void UdpProxy::proxyThreadMain()
{
    char buf[4096];

    while (!m_stopped)
    {
        nx::network::SocketAddress from;
        int bytesRead = m_socket.recvFrom(buf, sizeof(buf), &from);
        if (bytesRead < 0)
            continue;

        nx::network::SocketAddress to;
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            auto it = m_rules.find(from);
            if (it == m_rules.end())
                continue;
            to = it->second;
        }

        if (m_callback && !m_callback(buf, bytesRead))
            continue; //< drop packet.

        m_socket.sendTo(buf, bytesRead, to);
    }
}
