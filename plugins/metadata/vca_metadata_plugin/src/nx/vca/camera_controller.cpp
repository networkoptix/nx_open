#include "camera_controller.h"

#include <algorithm>

#include <nx/network/http/http_client.h>
#include <nx/network/socket_global.h>
#include <nx/kit/debug.h>


/*
    This unit is based on the "IPN HTTP API Manual" that explains how to use HTTP-based API of IPN
    series.

    CGI commands have the following format:
    "/nvc-cgi/admin/param.fcgi?action=<A>&group=<G>"
    or
    "/nvc-cgi/admin/param.fcgi?action=<A>&group=<G>&<prop>=<value>"


    <A> is for action. They are
    list – get the configuration values
    update - set the values
    add - add the setting value
    remove - remove the setting value
    query - get the available value range


    <G> is for group. Some of available groups (there are about 30 of them) are
    system
    brand
    version
    user
    network
    eventprofile  <- this group is key one for the purpose of analytics
    event
    ...


    "action=list" examples

    get name of profile P0
    "action=list&group=eventprofile.P0.name"

    get name and description of profile P0
    "action=list&group=eventprofile.P0.name,eventprofile.P0.description"

    get all profiles' names
    "action=list&group=eventprofile.P*.name"

    get all parameters of profile P0 (both so the same)
    "action=list&group=eventprofile.P0.*"
    "action=list&group=eventprofile.P0"

    get all profiles' names, descriptions and if they are enable
    "action=list&group=eventprofile.P*.name,eventprofile.P*.description,eventprofile.P*.enable"

    get all properties of all profiles (can take significant time)
    "action = list & group = eventprofile.P*.*"

    get all subproperties of Tcp property (though there is only one - it is "enable")
    "action=list&group=eventprofile.P1.Notification.Tcp"


    "action=query" examples

    ask for acceptable values of "eventprofile.P1.Notification.Tcp.enable"
    "action=query&group=eventprofile.P1.Notification.Tcp.enable"
    possible answer:
    "Eventprofile.P1.Notification.Tcp.enable=s__ul|yesno"
    means that property may be updated and listed and acceptable values are "yes" and "no"
    answer format is described in "IPN HTTP API Manual", paragraph 2.3 Data Format, page 17


    ask for acceptable values of all properties of "eventprofile.P1.Notification.Http"
    "action=query&group=eventprofile.P1.Notification.Http"


    "action=update" examples

    switch on profile P2 (i.e. make the rule P2 enable)
    "action=update&group=eventprofile.P2&enable=yes"

    switch on "Send notification via TCP event server"
    "action=update&group=eventprofile.P0.Notification.tcp&enable=yes"


    SOME USEFUL COMMANDS
    1. Obtain all supported groups and their properties.
    "action=list&group=*"
    or
    "action=query&group=*"
    Second command returns larger catalog, as it additionally provides write-only properties
    (e.g. AVIRec.instantrec). Also it provides more information about properties.
    Useful for study. Camera IPN352HDIR returns about 900 properties in about 30 groups.
    Caveat: command is very slow(dozen of seconds and longer).

    2. Reboot camera
   "action=update&group=System.Power&control=warmboot"
    For camera IPN352HDIR reboot takes about a minute.
    Reboot preserves all previously saved settings.

    3. Reset settings to default
   "action=update&group=system.factorydefault&control=hardware"
   "action=update&group=system.factorydefault&control=software"
    WARNING! All settings will be lost. You'd better export settings before reset.
    Reset with "control=software" is less aggressive. It preserves network address, time zone
    and user information.

    4. Ask for acceptable values of notifying tcp-server.
   "action=query&group=Event.Notify.tcp"
    Useful for examining camera tcp-server settings.

    5. Get port number of tcp server (that notifies about events).
   "action=list&group=Event.Notify.tcp.listenport"
    Captain Obvious affirms, that the command is necessary to establish tcp connection.

    6. Switch on "Send notification via TCP event server"
   "action=update&group=eventprofile.P0.Notification.tcp&enable=yes"
    Allows to read notifications from camera tcp-server.

    7. Switch heartbeat mode on & and set heartbeat parameters.
   "action=update&group=Event.Rule.health&tcp=yes&enable=yes&interval=10"
    "tcp=yes" enables heartbeat mode, "enable=yes" enables periodical heartbeat,
    "interval=<value>" set inteval in seconds in range [1..300]

    8. Switch rules on and off
   "action=update&group=Eventprofile.P0&enable=yes"
    "action=update&group=Eventprofile.P0&enable=no"
    Key command. Allows to enable/disable analytic rules.
*/

namespace nx {
namespace vca {

namespace {

static const int kPropertyOffset = sizeof("Eventprofile.PX.") - 1;
static const int kIdOffset = sizeof("Eventprofile.P") - 1;
static const int kMaxRuleCount = 8;
static_assert(kMaxRuleCount < 10, "rule profile number may have one digit");


bool isResponseOK(const nx_http::HttpClient& client)
{
    const nx_http::Response* response = client.response();
    if (!response)
        return false;
    return response->statusLine.statusCode == nx_http::StatusCode::ok;
}

std::vector<std::string> getRuleTableLines(QByteArray& ruleTable)
{
/*
    There can be no more then 8 rules. Each rule property takes one line.
    Lines are separated with \r\n.
    The example of possible rule table presented below:
    Eventprofile.P0.description=First VCA Rule
    Eventprofile.P1.description=Motion detection rule
    Eventprofile.P2.description=
    Eventprofile.P3.description=
    Eventprofile.P0.enable=yes
    Eventprofile.P1.enable=no
    Eventprofile.P2.enable=no
    Eventprofile.P3.enable=yes
    Eventprofile.P0.name=Rule - VCA
    Eventprofile.P1.name=Rule_2 (motion detected)
    Eventprofile.P2.name=FD Rule
    Eventprofile.P3.name=Rule 4 (one more VCA)
*/

    ruleTable.replace('\n', '\0'); //< Now every line is a null-terminated string.
    const char* begin = ruleTable.begin();
    const char* end = ruleTable.begin() + ruleTable.size();

    std::vector<std::string> lines; //< vector<string_view> would be perfect, unfortunately we have
                                    // not it yet... waiting for c++17
    const char* cur = begin;
    while (cur != end)
    {
        const char* lineBegin = cur;

        // This loop is safe as we know that QByteArray::data always have termination 0
        while (*cur)
            ++cur;

        if (cur>lineBegin)
            lines.emplace_back(lineBegin, cur - 1); //< lines end with '\r', so we cut it off

        if (cur != end)
            ++cur; //< go to next line
    }
    return lines;
}


//
//const QList<QByteArray> processData(const QByteArray& data)
//{
//    QList<QByteArray> result;
//    ruleTable.replace('\n', '\0'); //< Now every line is a null-terminated string.
//    for (auto line: data.split('\n'))
//    {
//        line = line.trimmed();
//
//    }
//}
//
















std::map<int, SupportedRule> parseRuleTableLines(const std::vector<std::string>& lines)
{

    std::map<int, SupportedRule> rules;
    for (const auto& line : lines)
    {
        if (line.size() < kPropertyOffset)
            continue; //< line is corrupted (too short)
        int ruleId = line[kIdOffset] - '0';
        if (ruleId >= kMaxRuleCount || ruleId < 0)
            continue; //< line is corrupted (ruleId can't exceed 7)

        size_t valueDisp = line.find('=', kPropertyOffset);
        if (valueDisp == std::string::npos)
            continue; //< line is corrupted (no '=' present)

        std::string value(line.begin() + valueDisp + 1, line.end()); //< Possible empty value is ok
        // if it is not a value of name (we filter out unnamed rules later).
        switch (line[kPropertyOffset])
        {
            case 'd':
                rules[ruleId].description = value;
                break;
            case 'e':
                rules[ruleId].ruleEnabled = (value == "yes");
                break;
            case 'n':
                rules[ruleId].name = value;
                rules[ruleId].profileId = static_cast<int>(ruleId);
                break;
            case 'N':
                rules[ruleId].tcpServerNotificationEnabled = (value == "yes");
                break;
        }
    }

    auto it = rules.begin();
    while (it != rules.end())
    {
        if (it->second.name.empty())
            rules.erase(it++);
        else
            ++it;

    }
    return rules;
}

std::map<int, bool> parseTcpTableLines(const std::vector<std::string>& lines)
{
    std::map<int, bool> result;
    for (const auto& line : lines)
    {
        if (line.size() < kPropertyOffset)
            continue; //< line is corrupted (too short)
        int ruleId = line[kIdOffset] - '0';
        if (ruleId >= kMaxRuleCount || ruleId < 0)
            continue; //< line is corrupted (ruleId can't exceed 7)

        size_t valueDisp = line.find('=', kPropertyOffset);
        if (valueDisp == std::string::npos)
            continue; //< line is corrupted (no '=' present)

        std::string value(line.begin() + valueDisp + 1, line.end()); //< Possible empty value is ok
        // if it is not a value of name (we filter out unnamed rules later).

        result[ruleId] = (value == "yes");
    }

    return result;
}

} // namespace

class CameraControllerImpl
{
    nx::network::SocketGlobals::InitGuard m_initGuard;
    nx_http::HttpClient m_client;
    QString m_cgiPreamble;
    static const QString kProtocol;
    static const QString kPath;

public:
    void setCgiPreamble(const std::string& ip)
    {
        m_cgiPreamble = kProtocol + QString::fromUtf8(ip.c_str()) + kPath;
    }
    void setUserPassword(const std::string& user, const std::string& password)
    {
        m_client.setUserName(user.c_str());
        m_client.setUserPassword(password.c_str());
    }
    bool execute(const QString& command, QByteArray& report)
    {
        QString url = m_cgiPreamble + command;

        if (!m_client.doGet(url))
            return false;

        if (!m_client.response() ||
            m_client.response()->statusLine.statusCode != nx_http::StatusCode::ok)
            return false;

        while (!m_client.eof())
            report.append(m_client.fetchMessageBodyBuffer());

        return true;
    }

    //client.setResponseReadTimeoutMs(10000);
    //client.setSendTimeoutMs(10000);
    //client.setMessageBodyReadTimeoutMs(10000);
};
const QString CameraControllerImpl::kProtocol("http://");
const QString CameraControllerImpl::kPath("/nvc-cgi/admin/param.fcgi?");

CameraController::CameraController(): m_impl(new CameraControllerImpl) {}

CameraController::CameraController(const char* ip):
    m_ip(ip ? ip : ""),
    m_impl(new CameraControllerImpl)
{
    m_impl->setCgiPreamble(m_ip);
}

CameraController::CameraController(const char* ip, const char* user, const char* password):
    m_ip(ip ? ip : ""),
    m_user(user ? user : ""),
    m_password(password ? password : ""),
    m_impl(new CameraControllerImpl)
{
    m_impl->setCgiPreamble(m_ip);
    m_impl->setUserPassword(m_user, m_password);
}

void CameraController::setIp(const char* ip)
{
    m_ip = ip ? ip : "";
    m_impl->setCgiPreamble(m_ip);
}

void CameraController::setUserPassword(const char* user, const char* password)
{
    m_user = user ? user : "";
    m_password = password ? password : "";
    m_impl->setUserPassword(m_user, m_password);
}

bool CameraController::readSupportedRules()
{
    static const QString kGetRulesCommand =
        "action=list&group=eventprofile.P*.name"
        ",eventprofile.P*.description"
        ",eventprofile.P*.enable"
        ",eventprofile.P*.Notification.Tcp.enable"
        ;

    QByteArray ruleTable;
    if (!m_impl->execute(kGetRulesCommand, ruleTable))
        return false;

    if (ruleTable.isEmpty())
        return false; //< response can't be empty, smth went wrong

    static const std::string kNotFound = "not found group parameter";
    if (ruleTable[0] == '#' &&
        std::search(ruleTable.cbegin(), ruleTable.cend(), kNotFound.cbegin(), kNotFound.cend()) !=
        ruleTable.cend())
    {
        // Camera have no rules at the moment.
        m_supportedRules.clear();
        return true;
    }

    std::vector<std::string> lines = getRuleTableLines(ruleTable);
    if (lines.empty())
        return false;

    std::map<int, SupportedRule> rules = parseRuleTableLines(lines);
    if (rules.empty())
        return false;

    m_supportedRules = std::move(rules);
    return true;
}

bool CameraController::readSupportedRules2()
{
    static const int kGetRulesCommandCound = 4;
    static const QString kGetRulesCommand[kGetRulesCommandCound] =
    {
        "action=list&group=eventprofile.P*.name",
        "action=list&group=eventprofile.P*.description",
        "action=list&group=eventprofile.P*.enable",
        "action=list&group=eventprofile.P*.Notification.Tcp.enable"
    };

    std::vector<std::string> lines;
    for (int i = 0; i < kGetRulesCommandCound; ++i)
    {
        QByteArray ruleTable;
        if (!m_impl->execute(kGetRulesCommand[i], ruleTable))
            continue;

        if (ruleTable.isEmpty())
            continue; //< response can't be empty, smth went wrong

        static const std::string kNotFound = "not found group parameter";
        if (ruleTable[0] == '#' &&
            std::search(ruleTable.cbegin(), ruleTable.cend(), kNotFound.cbegin(), kNotFound.cend()) !=
            ruleTable.cend())
        {
            // Camera have no rules at the moment.
            m_supportedRules.clear();
            return true;
        }

        std::vector<std::string> parameterLines = getRuleTableLines(ruleTable);
        lines.insert(lines.end(), parameterLines.cbegin(), parameterLines.cend());
    }

    if (lines.empty())
        return false;

    std::map<int, SupportedRule> rules = parseRuleTableLines(lines);
    if (rules.empty())
        return false;

    m_supportedRules = std::move(rules);
    return true;
}

bool CameraController::setHeartbeat(Heartbeat heartbeat) const
{
    const QString yesno = heartbeat.enabled ? "yes" : "no";
    const QString interval = QString::number(heartbeat.seconds);
    static const QString kSetHeartbeatCommandPattern =
        "action=update&group=Event.Rule.health&tcp=yes&enable=%1&interval=%2";
    const QString setHeartbeatCommand = kSetHeartbeatCommandPattern.arg(yesno, interval);

    QByteArray cgiResponse;
    m_impl->execute(setHeartbeatCommand, cgiResponse);

    return cgiResponse.startsWith("#200");
}


QByteArray extractCgiResponseValue(const QByteArray& cgiResponse)
{
    // Correct cgiResponse is smth like "Event.Notify.tcp.listenport=2555\r\n\0".
    // We need substring between '=' and '\r'.

    int equalSignPositionPlusOne = cgiResponse.indexOf('=') + 1;
    if (equalSignPositionPlusOne == 0)
        return QByteArray(); //<< incorrect cgi response
    int crSignPosition = cgiResponse.indexOf('\r', equalSignPositionPlusOne);

    // If there is no '\r' sign (that is format violation) we'll search till the end (it is safe).
    return cgiResponse.mid(equalSignPositionPlusOne,
        crSignPosition - equalSignPositionPlusOne);
}

bool CameraController::readTcpServerPort()
{
    static const QString kReadTcpServerPortCommand =
        "action=list&group=Event.Notify.tcp.listenport";

    QByteArray cgiResponse;
    m_impl->execute(kReadTcpServerPortCommand, cgiResponse);
    QByteArray value = extractCgiResponseValue(cgiResponse);

    int portNumber = value.toInt();
    if (portNumber == 0)
        return false; //<< incorrect cgi response

    m_tcpServerPort = static_cast<unsigned short>(portNumber);
    return true;
}

bool CameraController::readTcpServerEnabled(int eventProfileIndex)
{
    static const QString kReadTcpServerEnabledCommandPattern =
        "action=list&group=eventprofile.P%1.Notification.tcp.enable";

    const QString readTcpServerEnabledCommand =
        kReadTcpServerEnabledCommandPattern.arg(eventProfileIndex);

    QByteArray cgiResponse;
    m_impl->execute(readTcpServerEnabledCommand, cgiResponse);
    if (cgiResponse.startsWith("#"))
        return false; //< correct answer is <param>=<value>

    QByteArray value = extractCgiResponseValue(cgiResponse);
    bool enabled = (value.toStdString() == "yes");
    m_supportedRules[eventProfileIndex].tcpServerNotificationEnabled = enabled;

    return true;
}

bool CameraController::readTcpServerEnabled()
{
    static const QString kReadTcpServerEnabledCommand =
        "action=list&group=eventprofile.P*.Notification.tcp.enable";

    QByteArray tcpTable;

    m_impl->execute(kReadTcpServerEnabledCommand, tcpTable);
    if (tcpTable.startsWith("#"))
        return false; //< correct answer is a number of <param>=<value> lines

    std::vector<std::string> lines = getRuleTableLines(tcpTable);
    if (lines.empty())
        return false;

    std::map<int, bool> enabledTcps = parseTcpTableLines(lines);
    for (const auto& enabledTcp : enabledTcps)
    {
        auto it = m_supportedRules.find(enabledTcp.first);
        if (it != m_supportedRules.end())
            it->second.tcpServerNotificationEnabled = enabledTcp.second;

    }

    return true;
}

bool CameraController::setTcpServerEnabled(int eventProfileIndex, bool enabled)
{
    const QString yesno = enabled ? "yes" : "no";
    static const QString kSetTcpServerEnabledCommandPattern =
        "action=update&group=eventprofile.P%1.Notification.tcp&enable=%2";
    const QString setTcpServerEnabledCommand =
        kSetTcpServerEnabledCommandPattern.arg(QString::number(eventProfileIndex), yesno);

    QByteArray cgiResponse;
    m_impl->execute(setTcpServerEnabledCommand, cgiResponse);
    bool result = cgiResponse.startsWith("#200");

    m_supportedRules[eventProfileIndex].tcpServerNotificationEnabled = enabled;
    return result;
}

} // namespace vca
} // namespace nx

//action=update&group=eventprofile.P0.Notification.tcp&enable
