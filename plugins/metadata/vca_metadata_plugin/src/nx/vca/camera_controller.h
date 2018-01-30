#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace nx {
namespace vca {

struct SupportedRule
{
    std::string name;
    std::string description;
    int profileId=0; //< VCA-camera rules are stored in profiles with numbers in the range 0..7
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
        ruleEnabled(ruleEnabled),
        tcpServerNotificationEnabled(tcpServerNotificationEnabled)
    {
    }

    std::string profileName() const { return std::string("P") + std::to_string(profileId); }
    bool isActive() const { return ruleEnabled & tcpServerNotificationEnabled; }
};

struct Heartbeat
{
    int seconds;
    bool enabled;
    Heartbeat(int seconds, bool enabled): seconds(seconds < 300 ? seconds : 300), enabled(enabled) {}
};

class CameraControllerImpl;

class CameraController
{
public:
    CameraController();
    CameraController(const char* ip);
    CameraController(const char* ip, const char* user, const char* password);
    ~CameraController() = default;

    void setIp(const char* ip);
    void setUserPassword(const char* user, const char* password);

    const char* ip() const noexcept { return m_ip.c_str(); }
    const char* user() const noexcept { return m_user.c_str(); }
    const char* password() const noexcept { return m_password.c_str(); }

    const std::map<int, SupportedRule>& suppotedRules() const noexcept
    {
        return m_supportedRules;
    }
    unsigned short tcpServerPort() const noexcept { return m_tcpServerPort; }

    /*
     * readSupportedRules & readSupportedRules2 do the same thing - they read all necessary
     * information about rules on the camera. Because of problems with HttpClient (during reading
     * incorrect data from camera) readSupportedRules sometimes can't obtain rule information.
     * readSupportedRules2 uses workaround and works fine (but slow). When HttpClient will be fixed
     * readSupportedRules2 will become deprecated. Till that moment readSupportedRules is not
     * recommended to be used.
     */
    bool readSupportedRules();
    bool readSupportedRules2();

    bool setHeartbeat(Heartbeat heartbeat) const;
    bool readTcpServerPort();

    bool readTcpServerEnabled(int eventProfileIndex);
    bool readTcpServerEnabled();

    bool setTcpServerEnabled(int eventProfileIndex, bool enabled);

private:
    std::string m_ip;
    std::string m_user;
    std::string m_password;

    std::map<int, SupportedRule> m_supportedRules;
    unsigned short m_tcpServerPort = 0;

    std::shared_ptr<CameraControllerImpl> m_impl;
};

} // namespace vca
} // namespace nx
