#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtCore/QByteArray>

#include <nx/utils/log/assert.h>
#include <nx/utils/mac_address.h>

#include "test_camera_ini.h"

namespace nx::vms::testcamera {

/**
 * Stores the data for a part of the discovery response message corresponding to a single camera.
 * Immutable.
 */
class CameraDiscoveryResponse
{
public:
    CameraDiscoveryResponse(nx::utils::MacAddress macAddress):
        m_macAddress(std::move(macAddress))
    {
        NX_ASSERT(!m_macAddress.isNull());
    }

    /**
     * Parses the discovery response message part representing a camera.
     * @param outErrorMessage On error, receives the message in English, otherwise, remains intact.
     */
    CameraDiscoveryResponse(
        const QByteArray& cameraDiscoveryResponseMessagePart, QString* outErrorMessage);

    nx::utils::MacAddress macAddress() const { return m_macAddress; }

    QByteArray makeCameraDiscoveryResponseMessagePart() const;

private:
    void parseCameraDiscoveryResponseMessagePart(
        const QByteArray& cameraDiscoveryResponseMessagePart, QString* outErrorMessage);

private:
    nx::utils::MacAddress m_macAddress;
};

// TODO: #mshevchenko: Replace VERBOSE with DEBUG where appropriate.

/**
 * Represents the testcamera discovery response message and (de)serializes it using the format:
 * ```
 *     <port>{;<macAddress>}
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

    int port() const { return m_mediaPort; }

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
