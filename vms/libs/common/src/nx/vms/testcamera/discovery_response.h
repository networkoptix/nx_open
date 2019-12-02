#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtCore/QByteArray>

#include <nx/utils/mac_address.h>

namespace nx::vms::testcamera {

/**
 * Stores the data for a part of the discovery response message corresponding to a single camera.
 * Immutable.
 */
class CameraDiscoveryResponse
{
public:
    /** @param videoLayoutString Must be empty if video layout is not used for this camera. */
    CameraDiscoveryResponse(nx::utils::MacAddress macAddress, QByteArray videoLayoutString):
        m_macAddress(std::move(macAddress)), m_videoLayoutString(std::move(videoLayoutString))
    {
        NX_ASSERT(!m_macAddress.isNull());
        NX_ASSERT(!m_videoLayoutString.contains(';'));
        NX_ASSERT(!m_videoLayoutString.contains('/'));
    }

    /**
     * Parses the discovery response message part representing a camera.
     * @param outErrorMessage On error, receives the message in English, otherwise, remains intact.
     */
    CameraDiscoveryResponse(
        const QByteArray& cameraDiscoveryResponseMessagePart, QString* outErrorMessage);

    nx::utils::MacAddress macAddress() const { return m_macAddress; }

    /**
     * @return Video layout string in the Server property format: can be deserialized by
     *     QnCustomResourceVideoLayout.
     */
    QString videoLayoutSerialized() const
    {
        return /* temp QByteArray */ QByteArray(m_videoLayoutString).replace('&', ';');
    }

    QByteArray makeCameraDiscoveryResponseMessagePart() const;

private:
    void parseCameraDiscoveryResponseMessagePart(
        const QByteArray& cameraDiscoveryResponseMessagePart, QString* outErrorMessage);

private:
    nx::utils::MacAddress m_macAddress;
    QByteArray m_videoLayoutString;
};

/**
 * Represents the testcamera discovery response message and (de)serializes it using the format:
 * ```
 *     <port>{;<macAddress>[/<videoLayout>]}
 * ```
 */
class DiscoveryResponse
{
public:
    DiscoveryResponse(int mediaPort): m_mediaPort(mediaPort)
    {
        NX_ASSERT(m_mediaPort >= 1 && m_mediaPort <= 65535, "Invalid media port: %1", mediaPort);
    }

    /**
     * Parses the discovery response message received from testcamera by the Server.
     * @param outErrorMessage On error, receives the message in English, otherwise, remains intact.
     */
    DiscoveryResponse(const QByteArray& discoveryResponseMessage, QString* outErrorMessage);

    int mediaPort() const { return m_mediaPort; }

    const std::vector<std::shared_ptr<CameraDiscoveryResponse>>& cameraDiscoveryResponses() const
    {
        return m_cameraDiscoveryResponses;
    }

    void addCameraDiscoveryResponse(
        std::shared_ptr<CameraDiscoveryResponse> cameraDiscoveryResponse);

    /** Makes the discovery response message which is transmitted from testcamera to the Server. */
    QByteArray makeDiscoveryResponseMessage() const;

private:
    void parseDiscoveryResponseMessage(
        const QByteArray& discoveryResponseMessage, QString* outErrorMessage);

private:
    int m_mediaPort = -1;
    std::vector<std::shared_ptr<CameraDiscoveryResponse>> m_cameraDiscoveryResponses;
};

} // namespace nx::vms::testcamera
