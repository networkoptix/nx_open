#pragma once

#include <functional>
#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QStringList>

#include <nx/utils/thread/long_runnable.h>
#include <utils/math/math.h>

namespace nx::network
{
    class AbstractDatagramSocket;
    class SocketAddress;
}

namespace nx::vms::testcamera {

class Logger;

/**
 * Discovery service: runs the listener thread which receives discovery messages and sends
 * responses.
 */
class CameraDiscoveryListener: public QnLongRunnable
{
public:
    CameraDiscoveryListener(
        const Logger* logger,
        std::function<QByteArray()> obtainDiscoveryResponseMessageFunc,
        QStringList localInterfacesToListen);

    virtual ~CameraDiscoveryListener();

    bool initialize();

private:
    struct IpRangeV4
    {
        quint32 firstIp = 0;
        quint32 lastIp = 0;
        bool contains(quint32 addr) const { return qBetween(firstIp, addr, lastIp); }
    };

    bool serverAddressIsAllowed(const nx::network::SocketAddress& serverAddress);

    QByteArray receiveDiscoveryMessage(
        QByteArray* buffer,
        nx::network::SocketAddress* outServerAddress,
        nx::network::AbstractDatagramSocket* discoverySocket) const;

    void sendDiscoveryResponseMessage(
        nx::network::AbstractDatagramSocket* discoverySocket,
        const nx::network::SocketAddress& serverAddress) const;

protected:
    virtual void run() override;

private:
    const Logger* const m_logger;
    const std::function<QByteArray()> m_obtainDiscoveryResponseMessageFunc;
    const QStringList m_localInterfacesToListen;
    std::vector<IpRangeV4> m_allowedIpRanges;
};

} // namespace nx::vms::testcamera
