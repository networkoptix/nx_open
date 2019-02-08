#pragma once

#include <vector>
#include <map>
#include <memory>
#include <chrono>

#include <QList>
#include <QByteArray>

#include <nx/network/http/http_types.h>

namespace nx {
namespace dw_mtt {

/*
 * Make xml body for subscruption request.
 * @param items - list of internal names of the events to subscribe to.
*/
QByteArray makeSubscriptionXml(const QSet<QByteArray>& items);

class CameraControllerImpl;

class CameraController
{
public:
    CameraController();
    CameraController(const QByteArray& ip);
    CameraController(const QByteArray& ip, const QByteArray& user, const QByteArray& password);
    ~CameraController() = default;

    void setIp(const QByteArray& ip);
    void setCredentials(const QByteArray& user, const QByteArray& password);
    void setReadTimeout(std::chrono::seconds readTimeout);

    QByteArray ip() const noexcept { return m_ip; }
    QByteArray user() const noexcept { return m_user; }
    QByteArray password() const noexcept { return m_password; }

    /** Get camera's tcp ports. */
    bool readPortConfiguration();

    uint16_t httpPort() const noexcept { return m_httpPort; }
    uint16_t netPort() const noexcept { return m_netPort; }
    uint16_t rtspPort() const noexcept { return m_rtspPort; }
    uint16_t httpsPort() const noexcept { return m_httpsPort; }
    uint16_t longPollingPort() const noexcept { return m_longPollingPort; }

    /** Make http request that subscribes to events, enumerated in the body. */
    nx::network::http::Request makeHttpRequest(const QByteArray& body);
private:
    QByteArray m_ip;
    QByteArray m_user;
    QByteArray m_password;

    std::shared_ptr<CameraControllerImpl> m_impl;

    uint16_t m_httpPort = 0;
    uint16_t m_netPort = 0;
    uint16_t m_rtspPort = 0;
    uint16_t m_httpsPort = 0;
    uint16_t m_longPollingPort = 0;
};

} // namespace dw_mtt
} // namespace nx
