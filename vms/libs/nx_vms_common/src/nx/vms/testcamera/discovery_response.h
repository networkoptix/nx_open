// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

#include <nx/utils/log/assert.h>
#include <nx/utils/mac_address.h>

namespace nx::vms::testcamera {

/**
 * Stores the data for a part of the discovery response message corresponding to a single camera.
 * Immutable.
 */
class NX_VMS_COMMON_API CameraDiscoveryResponse
{
public:
    /** @param videoLayoutString Must be empty if video layout is not used for this camera. */
    CameraDiscoveryResponse(
        nx::utils::MacAddress macAddress,
        QByteArray videoLayoutString,
        QJsonObject payload)
        :
        m_macAddress(std::move(macAddress)),
        m_videoLayoutString(std::move(videoLayoutString)),
        m_payload(std::move(payload))
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

    /** @return Additional data for QnTestCameraResourceSearcher. */
    QJsonObject payload() const
    {
        return m_payload;
    }

    QByteArray makeCameraDiscoveryResponseMessagePart() const;

private:
    void parseCameraDiscoveryResponseMessagePart(
        const QByteArray& cameraDiscoveryResponseMessagePart, QString* outErrorMessage);

    static QByteArray encodeJson(const QJsonObject& json);
    static QJsonObject decodeJson(const QByteArray& data, QJsonParseError* outError);

private:
    nx::utils::MacAddress m_macAddress;
    QByteArray m_videoLayoutString;
    QJsonObject m_payload;
};

/**
 * Represents the testcamera discovery response message and (de)serializes it using the format:
 * ```
 *     <port>{;<macAddress>[/<videoLayout>[/<payload>]]}
 * ```
 * where
 * `<videoLayout>` contains sensors layout on camera (example: `width=3&height=1&sensors=0,1,2`),
 * `<payload>` contains arbitrary data in JSON format with bytes `;`, `/` and `%` encoded using
 * percent encoding (like in URL parameters).
 */
class NX_VMS_COMMON_API DiscoveryResponse
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
