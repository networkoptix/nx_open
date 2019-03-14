#include "generic_multicast_io_device.h"

#include <QtCore/QtEndian>

#include <nx/utils/log/assert.h>
#include <nx/utils/std/cpp14.h>

#include <nx/network/system_socket.h>

namespace {

const std::size_t kBufferCapacity = 32 * 1024;
const std::chrono::milliseconds kReceiveTimeout(5000);
const int kFixedRtpHeaderLength = 12;
const char kRtpHeaderMask = 0b01000000;
const char kRtpExtensionMask = 0b00010000;
const int kExtensionHeaderIdLength = 2;
const int kExtensionLengthFieldLength = 2;

} // namespace


GenericMulticastIoDevice::GenericMulticastIoDevice(const QUrl& url):
    m_url(url)
{

}

GenericMulticastIoDevice::~GenericMulticastIoDevice()
{
    for (const auto iface : nx::network::getAllIPv4Interfaces())
        m_socket->leaveGroup(m_url.host(), iface.address.toString());
}

bool GenericMulticastIoDevice::open(QIODevice::OpenMode openMode)
{
    NX_ASSERT(
        openMode == QIODevice::ReadOnly,
        lm("Multicast IO device can be opened only in readonly mode."));

    if (!initSocket(m_url))
        return false;

    return QIODevice::open(openMode);
}

bool GenericMulticastIoDevice::isSequential() const
{
    return true;
}

qint64 GenericMulticastIoDevice::readData(char* buf, qint64 maxlen)
{
    auto received = m_socket->recv(buf, maxlen);
    if (received <= 0)
    {
        QIODevice::close();
        return -1;
    }
    return received;
}

qint64 GenericMulticastIoDevice::writeData(const char* /*buf*/, qint64 /*size*/)
{
    NX_ASSERT(false, lm("We should not write data to multicast IO device."));
    return 0;
}

bool GenericMulticastIoDevice::initSocket(const QUrl& url)
{
    bool result = true;

    m_socket = std::make_unique<nx::network::UDPSocket>();

    result &= m_socket->setReuseAddrFlag(true);
    result &= m_socket->setRecvBufferSize(kBufferCapacity);
    result &= m_socket->setRecvTimeout(kReceiveTimeout.count());
    result &= m_socket->bind(nx::network::SocketAddress(nx::network::HostAddress::anyHost, url.port()));

    if (!result)
        return false;

    for (const auto iface : nx::network::getAllIPv4Interfaces())
        result &= m_socket->joinGroup(url.host(), iface.address.toString());

    return result;
}

qint64 GenericMulticastIoDevice::cutRtpHeaderOff(const char* inBuf, qint64 inBufLength, char* const outBuf, qint64 outBufLength)
{
    if (inBufLength < 1)
        return 0;

    // If it's not rtp packet
    if (!(inBuf[0] & kRtpHeaderMask))
    {
        if (outBufLength < inBufLength)
            return 0;

        memcpy(outBuf, inBuf, inBufLength);
        return inBufLength;
    }

    if (inBufLength < kFixedRtpHeaderLength)
        return 0;

    int rtpHeaderLength = kFixedRtpHeaderLength;
    bool hasExtension = inBuf[0] & kRtpExtensionMask;

    int payloadLength = inBufLength - rtpHeaderLength;
    inBuf += rtpHeaderLength;

    if (!hasExtension)
    {
        payloadLength = inBufLength - rtpHeaderLength;

        if (outBufLength < payloadLength)
            return 0;

        memcpy(outBuf, inBuf, payloadLength);
        return payloadLength;
    }

    payloadLength -= kExtensionHeaderIdLength + kExtensionLengthFieldLength;
    if (payloadLength < 1)
        return 0;

    inBuf += kExtensionHeaderIdLength;
    int extensionLength = qFromBigEndian<short>(*(short*)inBuf);

    payloadLength -= extensionLength;
    if (payloadLength < 1)
        return 0;

    inBuf += kExtensionLengthFieldLength;

    if (outBufLength < payloadLength)
        return 0;

    memcpy(outBuf, inBuf, payloadLength);
    return payloadLength;
}
