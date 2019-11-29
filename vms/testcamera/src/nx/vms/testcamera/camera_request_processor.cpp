#include "camera_request_processor.h"

#include <QtCore/QList>

#include <nx/kit/utils.h>
#include <network/tcp_connection_priv.h>

#include "camera_pool.h"
#include "logger.h"

namespace nx::vms::testcamera {

class CameraRequestProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
    using QnTCPConnectionProcessorPrivate::QnTCPConnectionProcessorPrivate;
};

CameraRequestProcessor::CameraRequestProcessor(
    CameraPool* cameraPool,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    bool noSecondaryStream,
    int fpsPrimary,
    int fpsSecondary)
    :
    QnTCPConnectionProcessor(std::move(socket), /*owner*/ cameraPool),
    m_logger(new Logger(lm("CameraRequestProcessor(%1)").args(getForeignAddress()))),
    m_cameraPool(cameraPool),
    m_noSecondaryStream(noSecondaryStream),
    m_fpsPrimary(fpsPrimary),
    m_fpsSecondary(fpsSecondary)
{
}

CameraRequestProcessor::~CameraRequestProcessor()
{
    stop();
}

QByteArray CameraRequestProcessor::receiveCameraUrl()
{
    Q_D(CameraRequestProcessor);

    quint8 buffer[128];
    if (m_needStop || !d->socket->isConnected())
        return QByteArray();

    const int bytesRead = d->socket->recv(buffer, sizeof(buffer));

    if (bytesRead == 0)
    {
        NX_LOGGER_INFO(m_logger, "No data received from Server; expected Camera URL.");
        return QByteArray();
    }

    if (bytesRead < 0)
    {
        NX_LOGGER_ERROR(m_logger, "Unable to receive Camera URL: %1",
            SystemError::getLastOSErrorText());
        return QByteArray();
    }

    QByteArray cameraUrl((const char*) buffer, bytesRead);

    NX_LOGGER_VERBOSE(m_logger, "Received Camera URL: %1", nx::kit::utils::toString(cameraUrl));

    if (!cameraUrl.isEmpty() && cameraUrl[0] == '/')
        cameraUrl = cameraUrl.mid(1);

    return cameraUrl;
}

void CameraRequestProcessor::run()
{
    Q_D(CameraRequestProcessor);

    const QByteArray cameraUrl = receiveCameraUrl();

    const int pos = cameraUrl.indexOf('?');
    const QList<QByteArray> params = cameraUrl.mid(pos+1).split('&');
    const QByteArray macAddressString = cameraUrl.left(pos);

    const nx::utils::MacAddress macAddress(macAddressString);
    if (macAddress.isNull())
    {
        NX_LOGGER_VERBOSE(m_logger, "Invalid MAC address %1 in received URL %2.",
            nx::kit::utils::toString(macAddressString), nx::kit::utils::toString(cameraUrl));
        return;
    }

    auto* const camera = m_cameraPool->findCamera(macAddress);
    if (camera == nullptr)
    {
        NX_LOGGER_VERBOSE(m_logger, "No Camera found with MAC %1.", macAddressString);
        return;
    }

    // Try to receive the values from the Client.
    StreamIndex streamIndex = StreamIndex::primary;
    int fps = 10;
    for (int i = 0; i < params.size(); ++ i)
    {
        const QList<QByteArray> paramVal = params[i].split('=');
        if (paramVal[0] == "primary" && paramVal.size() == 2)
        {
            if (paramVal[1] != "1")
                streamIndex = StreamIndex::secondary;
        }
        else if (paramVal[0] == "fps" && paramVal.size() == 2)
        {
            fps = paramVal[1].toInt();
        }
    }

    if (streamIndex == StreamIndex::secondary && m_noSecondaryStream)
    {
        NX_LOGGER_WARNING(m_logger, "Secondary stream requested in the URL but is not available.");
        return;
    }

    const int overridingFps =
        (streamIndex == StreamIndex::secondary) ? m_fpsSecondary : m_fpsPrimary;
    if (overridingFps != -1)
        fps = overridingFps;

    camera->performStreaming(d->socket.get(), streamIndex, fps);
}

} // namespace nx::vms::testcamera;
