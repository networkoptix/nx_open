#include "discovery_response.h"

#include <nx/kit/utils.h>
#include <nx/utils/log/log.h>

#include <nx/vms/testcamera/test_camera_ini.h>

namespace nx::vms::testcamera {

//-------------------------------------------------------------------------------------------------
// CameraDiscoveryResponse

CameraDiscoveryResponse::CameraDiscoveryResponse(
    const QByteArray& cameraDiscoveryResponseMessagePart, QString* outErrorMessage)
{
    if (!NX_ASSERT(outErrorMessage))
        return;

    parseCameraDiscoveryResponseMessagePart(cameraDiscoveryResponseMessagePart, outErrorMessage);
}

void CameraDiscoveryResponse::parseCameraDiscoveryResponseMessagePart(
    const QByteArray& cameraDiscoveryResponseMessagePart, QString* outErrorMessage)
{
    const auto error = //< Intended to be called like `return error("...");`.
        [outErrorMessage](const QString& message) { *outErrorMessage = message; };

    const QList<QByteArray> parts = cameraDiscoveryResponseMessagePart.split('/');

    if (parts.empty())
        return error("MAC address is missing.");
    if (parts.size() > 2)
        return error("Too many slash-separated components.");

    m_macAddress = nx::utils::MacAddress(parts.at(0));
    if (m_macAddress.isNull())
        return error(lm("Invalid MAC address: %1.").args(nx::kit::utils::toString(parts.at(0))));

    m_videoLayoutString = (parts.size() >= 2) ? parts.at(1) : "";
}

QByteArray CameraDiscoveryResponse::makeCameraDiscoveryResponseMessagePart() const
{
    return m_macAddress.toString().toLatin1()
        + (m_videoLayoutString.isEmpty() ? "" : ('/' + m_videoLayoutString));
}

//-------------------------------------------------------------------------------------------------
// DiscoveryResponse

DiscoveryResponse::DiscoveryResponse(
    const QByteArray& discoveryResponseMessage, QString* outErrorMessage)
{
    if (!NX_ASSERT(outErrorMessage))
        return;

    parseDiscoveryResponseMessage(discoveryResponseMessage, outErrorMessage);
}

void DiscoveryResponse::parseDiscoveryResponseMessage(
    const QByteArray& discoveryResponseMessage, QString* outErrorMessage)
{
    const auto error = //< Intended to be called like `return error("...", ...);`.
        [outErrorMessage](const QString& message, auto... args)
        {
            if constexpr (sizeof...(args) > 0)
                *outErrorMessage = lm(message).args(args...);
            else
                *outErrorMessage = message;
        };

    const QByteArray expectedDiscoveryResponsePrefix =
        ini().discoveryResponseMessagePrefix + QByteArray("\n;");

    if (!discoveryResponseMessage.startsWith(expectedDiscoveryResponsePrefix))
        return error("Unexpected discovery response message prefix.");

    const QList<QByteArray> parts =
        discoveryResponseMessage.mid(expectedDiscoveryResponsePrefix.size()).split(';');
    if (parts.size() < 2)
        return error("Must contain the port number and at least one MAC address.");

    bool success = false;
    m_mediaPort = parts[0].toInt(&success);
    if (!success)
        return error("Invalid port number: %1", nx::kit::utils::toString(parts[0]));
    if (m_mediaPort < 1 || m_mediaPort > 65535)
        return error("Port number should be in range [1, 65535] but %1 found.", m_mediaPort);

    for (int i = 1; i < parts.size(); ++i)
    {
        const auto cameraDiscoveryResponse =
            std::make_shared<CameraDiscoveryResponse>(parts[i], outErrorMessage);
        if (!outErrorMessage->isEmpty())
            return;

        m_cameraDiscoveryResponses.push_back(cameraDiscoveryResponse);
    }
}

void DiscoveryResponse::addCameraDiscoveryResponse(
    std::shared_ptr<CameraDiscoveryResponse> cameraDiscoveryResponse)
{
    m_cameraDiscoveryResponses.push_back(cameraDiscoveryResponse);
}

QByteArray DiscoveryResponse::makeDiscoveryResponseMessage() const
{
    QByteArray response = ini().discoveryResponseMessagePrefix;
    response += '\n';
    response += ';';
    response += QByteArray::number(m_mediaPort);

    for (const auto& cameraDiscoveryResponse: m_cameraDiscoveryResponses)
        response += ";" + cameraDiscoveryResponse->makeCameraDiscoveryResponseMessagePart();

    return response;
}

} // namespace nx::vms::testcamera
