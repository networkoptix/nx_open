#pragma once

#include <vector>
#include <map>
#include <memory>
#include <chrono>

#include <QString>

namespace nx {
namespace vca {

struct SupportedRule
{
    QByteArray name;
    QByteArray description;
    int profileId = 0; //< VCA-camera rules are stored in profiles with numbers in the range 0..7.
    bool ruleEnabled = false;
    bool tcpServerNotificationEnabled = false;

    SupportedRule() = default;

    SupportedRule(
        const char* name,
        const char* description,
        int profileId,
        bool enabled = false,
        bool tcpServerNotificationEnabled = false)
        :
        name(name ? name : ""),
        description(description ? description : ""),
        profileId(profileId),
        ruleEnabled(enabled),
        tcpServerNotificationEnabled(tcpServerNotificationEnabled)
    {
    }

    QByteArray profileName() const { return QByteArray("P") + QByteArray::number(profileId); }
    bool isActive() const { return ruleEnabled & tcpServerNotificationEnabled; }
};

struct Heartbeat
{
    std::chrono::seconds interval = std::chrono::seconds(1);
    bool isEnabled = false;
};

class CameraControllerImpl;

/**
 * Class to work with VCA-camera settings.
 *
 * Methods which names start with "read" obtain appropriate information from camera and save it
 * into class private members. Further information may be accessed with corresponding methods.
 *
 * All read methods are synchronous and may last quite long (depending on the request and camera
 * performance). Default read timeout is set to 10 seconds, longer requests will fail.
 * Read-timeout may be enlarged with setReadTimeout method. Timeout also may be reduces though
 * it is not recommended.
 *
 * All read methods return boolean value (true - in case of successful reading).
 */
class CameraController
{
public:
    CameraController();
    CameraController(const QString& ip);
    CameraController(const QString& ip, const QString& user, const QString& password);
    ~CameraController() = default;

    void setIp(const QString& ip);
    void setUserPassword(const QString& user, const QString& password);
    void setReadTimeout(std::chrono::seconds readTimeout);

    QString ip() const noexcept { return m_ip; }
    QString user() const noexcept { return m_user; }
    QString password() const noexcept { return m_password; }

    const std::map<int, SupportedRule>& suppotedRules() const noexcept
    {
        return m_supportedRules;
    }
    int16_t tcpServerPort() const noexcept { return m_tcpServerPort; }

    /**
     * readSupportedRules & readSupportedRules2 do the same thing - they read all necessary
     * information about rules on the camera and save it into the internal rule table of the
     * CameraController object.
     * This rule table may be accessed with suppotedRules() method.
     * readSupportedRules reads data at once, readSupportedRules2 reads data in chunks.
     */
    bool readSupportedRules();
    bool readSupportedRules2();

    /**
     * Read information about supported rules enable/disable state and update current rule table.
     */
    bool readSupportedRulesState();

    /** Read information about supported rules names and update current rule table. */
    bool readSupportedRulesNames();

    /** Read information about supported rules tcp server notification, if it is enabled or not. */
    bool readSupportedRulesTcpNotificationState();

    bool setHeartbeat(Heartbeat heartbeat) const;

    /** Read the number of tcp port through which the camera server sends notifications when events
     * occur. To obtain port number, use tcpServerPort() after readTcpServerPort().
     */
    bool readTcpServerPort();

    bool readTcpServerEnabled(int eventProfileIndex);

    bool setTcpServerEnabled(int eventProfileIndex, bool enabled);

private:
    bool readRules(const QString& cgiCommand, std::map<int, SupportedRule>& rules);

private:
    QString m_ip;
    QString m_user;
    QString m_password;

    std::map<int, SupportedRule> m_supportedRules;
    int16_t m_tcpServerPort = 0;

    std::shared_ptr<CameraControllerImpl> m_impl;
};

} // namespace vca
} // namespace nx
