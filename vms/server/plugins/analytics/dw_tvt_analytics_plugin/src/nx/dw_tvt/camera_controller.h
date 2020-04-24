#pragma once

#include <vector>
#include <map>
#include <memory>
#include <chrono>

#include <QList>
#include <QByteArray>

#include <nx/network/http/http_types.h>

namespace nx::dw_tvt {

/**
 * XML standard demands '\n' as a line separator ('\r\n' and '\r' are not acceptable)
 * (https://www.w3.org/TR/2008/REC-xml-20081126/#sec-line-ends),
 * DW TVT cameras follow this demand.
 */
    void normalizeEndlines(QByteArray* xml);

/**
 * Eliminates all indentations (whitespaces in the beginning of each line).
 * Converts "human convenient readable" format to "flat format".
 */
void unindentLines(QByteArray* xml);

/**
 * Make xml body for subscription request.
 * @param items - list of internal names of the events to subscribe to.
 */
QByteArray buildSubscriptionXml(const QSet<QByteArray>& items);

class CameraHttpClient;

class CameraController
{
public:
    CameraController();
    CameraController(const QByteArray& ip, unsigned short port);
    CameraController(const QByteArray& ip, unsigned short port,
        const QByteArray& user, const QByteArray& password);
    ~CameraController();

    void setIpPort(const QByteArray& ip, unsigned short port);
    void setCredentials(const QByteArray& user, const QByteArray& password);
    void setReadTimeout(std::chrono::seconds readTimeout);

    QByteArray ip() const noexcept { return m_ip; }
    unsigned short port() const noexcept { return m_port; }
    QByteArray user() const noexcept { return m_user; }
    QByteArray password() const noexcept { return m_password; }

    /** Get camera's tcp ports. */
    bool readPortConfiguration();

    uint16_t httpPort() const noexcept { return m_httpPort; }
    uint16_t netPort() const noexcept { return m_netPort; }
    uint16_t rtspPort() const noexcept { return m_rtspPort; }
    uint16_t httpsPort() const noexcept { return m_httpsPort; }
    uint16_t longPollingPort() const noexcept { return m_longPollingPort; }

private:
    QByteArray m_ip;
    unsigned short m_port = 0;
    QByteArray m_user;
    QByteArray m_password;

    std::unique_ptr<CameraHttpClient> m_cameraHttpClient;

    uint16_t m_httpPort = 0;
    uint16_t m_netPort = 0;
    uint16_t m_rtspPort = 0;
    uint16_t m_httpsPort = 0;
    uint16_t m_longPollingPort = 0;
};

} // namespace nx::dw_tvt
