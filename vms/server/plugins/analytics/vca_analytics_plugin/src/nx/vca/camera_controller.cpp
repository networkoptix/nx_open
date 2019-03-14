#include "camera_controller.h"

#include <algorithm>

#include <nx/network/http/http_client.h>
#include <nx/network/socket_global.h>
#include <nx/kit/debug.h>

/**@file
 * This unit is based on the "IPN HTTP API Manual" that explains how to use HTTP-based API of IPN
 * series.
 *
 * CGI commands have the following format:
 * "/nvc-cgi/admin/param.fcgi?action=<A>&group=<G>"
 * or
 * "/nvc-cgi/admin/param.fcgi?action=<A>&group=<G>&<prop>=<value>"
 *
 * <A> is for action. They are
 * list – get the configuration values
 * update - set the values
 * add - add the setting value
 * remove - remove the setting value
 * query - get the available value range
 *
 * <G> is for group. Some of available groups (there are about 30 of them) are
 * system
 * brand
 * version
 * user
 * network
 * eventprofile  <- this group is key one for the purpose of analytics
 * event
 * ...
 *
 * "action=list" examples
 *
 * get name of profile P0
 * "action=list&group=eventprofile.P0.name"
 *
 * get name and description of profile P0
 * "action=list&group=eventprofile.P0.name,eventprofile.P0.description"
 *
 * get all profiles' names
 * "action=list&group=eventprofile.P*.name"
 *
 * get all parameters of profile P0 (both so the same)
 * "action=list&group=eventprofile.P0.*"
 * "action=list&group=eventprofile.P0"
 *
 * get all profiles' names, descriptions and if they are enable
 * "action=list&group=eventprofile.P*.name,eventprofile.P*.description,eventprofile.P*.enable"
 *
 * get all properties of all profiles (can take significant time)
 * "action = list & group = eventprofile.P*.*"
 *
 * get all subproperties of Tcp property (though there is only one - it is "enable")
 * "action=list&group=eventprofile.P1.Notification.Tcp"
 *
 * "action=query" examples
 *
 * ask for acceptable values of "eventprofile.P1.Notification.Tcp.enable"
 * "action=query&group=eventprofile.P1.Notification.Tcp.enable"
 * possible answer:
 * "Eventprofile.P1.Notification.Tcp.enable=s__ul|yesno"
 * means that property may be updated and listed and acceptable values are "yes" and "no"
 * answer format is described in "IPN HTTP API Manual", paragraph 2.3 Data Format, page 17
 *
 * ask for acceptable values of all properties of "eventprofile.P1.Notification.Http"
 * "action=query&group=eventprofile.P1.Notification.Http"
 *
 * "action=update" examples
 *
 * switch on profile P2 (i.e. make the rule P2 enable)
 * "action=update&group=eventprofile.P2&enable=yes"
 *
 * switch on "Send notification via TCP event server"
 * "action=update&group=eventprofile.P0.Notification.tcp&enable=yes"
 *
 * SOME USEFUL COMMANDS
 * 1. Obtain all supported groups and their properties.
 * "action=list&group=*"
 * or
 * "action=query&group=*"
 * Second command returns larger catalog, as it additionally provides write-only properties
 * (e.g. AVIRec.instantrec). Also it provides more information about properties.
 * Useful for study. Camera IPN352HDIR returns about 900 properties in about 30 groups.
 *
 * 2. Reboot camera
 * "action=update&group=System.Power&control=warmboot"
 * For camera IPN352HDIR reboot takes about a minute.
 * Reboot preserves all previously saved settings.
 *
 * 3. Reset settings to default
 * "action=update&group=system.factorydefault&control=hardware"
 * "action=update&group=system.factorydefault&control=software"
 *  WARNING! All settings will be lost. You'd better export settings before reset.
 * Reset with "control=software" is less aggressive. It preserves network address, time zone
 * and user information.
 *
 * 4. Ask for acceptable values of notifying tcp-server.
 * "action=query&group=Event.Notify.tcp"
 * Useful for examining camera tcp-server settings.
 *
 * 5. Get port number of tcp server (that notifies about events).
 * "action=list&group=Event.Notify.tcp.listenport"
 * Captain Obvious affirms, that the command is necessary to establish tcp connection.
 *
 * 6. Switch on "Send notification via TCP event server"
 * "action=update&group=eventprofile.P0.Notification.tcp&enable=yes"
 * Allows to read notifications from camera tcp-server.
 *
 * 7. Switch heartbeat mode on & and set heartbeat parameters.
 * action=update&group=Event.Rule.health&tcp=yes&enable=yes&interval=10"
 * "tcp=yes" enables heartbeat mode, "enable=yes" enables periodical heartbeat,
 * "interval=<value>" set inteval in seconds in range [1..300]
 *
 * 8. Switch rules on and off
 * action=update&group=Eventprofile.P0&enable=yes"
 * "action=update&group=Eventprofile.P0&enable=no"
 * Key command. Allows to enable/disable analytic rules.
 */

namespace nx {
namespace vca {

namespace {

struct RuleTuple
{
    int number=-1;
    QByteArray parameter;
    QByteArray value;
};

QByteArrayList splitRuleTable(QByteArray& ruleTable)
{
    // Rule table consists on lines that end with \r\n.
    QByteArrayList result = ruleTable.split('\n');
    for (auto& line: result)
        line.chop(1); //< Delete last '\r' symbol.
    if (!result.isEmpty() && result.back().isEmpty())
        result.pop_back(); //< camera response often has final empty line
    return result;
}

RuleTuple extractParameter(const QByteArray& line)
{
    // The example of input line: "Eventprofile.P3.Notification.Tcp.enable=no".
    // Here ruleName is 3, ruleParameter is "Notification.Tcp.enable", ruleParameterValue is "yes".

    RuleTuple result;
    const int delimiterPos = line.indexOf('=');
    if (delimiterPos == -1)
        return RuleTuple();
    const auto key = line.left(delimiterPos);
    result.value = line.mid(delimiterPos + 1);
    auto keyParts = key.split('.');
    if (keyParts.size() < 3)
        return RuleTuple();
    result.number = keyParts[1].mid(1).toInt();

    keyParts.removeFirst();
    keyParts.removeFirst(); //< Remove 2 first elements.
    result.parameter = keyParts.join('.');
    return result;
}

void filterOutUnnamedRules(std::map<int, SupportedRule>& rules)
{
    for (auto it = rules.begin(); it != rules.end();)
    {
        if (it->second.name.isEmpty())
            rules.erase(it++);
        else
            ++it;
    }
}

std::map<int, SupportedRule> parseRuleTableLines(const QByteArrayList& lines)
{
    std::map<int, SupportedRule> rules;
    for (const auto& line: lines)
    {
        // There may be several '=' signs in a line, but we search leftmost.
        RuleTuple ruleTuple = extractParameter(line);
        if (ruleTuple.number == -1)
            continue;

        rules[ruleTuple.number].profileId = ruleTuple.number;
        if (ruleTuple.parameter == "description")
        {
            rules[ruleTuple.number].description = ruleTuple.value;
        }
        else if (ruleTuple.parameter == "enable")
        {
            rules[ruleTuple.number].ruleEnabled = (ruleTuple.value == "yes");
        }
        else if (ruleTuple.parameter == "name")
        {
            rules[ruleTuple.number].name = ruleTuple.value;
        }
        else if (ruleTuple.parameter == "Notification.Tcp.enable")
        {
            rules[ruleTuple.number].tcpServerNotificationEnabled = (ruleTuple.value == "yes");
        }
    }
    return rules;
}

std::map<int, bool> parseTcpTableLines(const QByteArrayList& lines)
{
    std::map<int, bool> result;
    for (const auto& line: lines)
    {
        RuleTuple ruleTuple = extractParameter(line);
        if(ruleTuple.number != -1 && ruleTuple.parameter == "Notification")
            result[ruleTuple.number] = (ruleTuple.value == "yes");
    }

    return result;
}

/** Check if CGI response contains no information about queried group of parameters. */
bool groupNotFound(const QByteArray& responseTable)
{
    static const QByteArray kNotFound = "not found group parameter";
    return !responseTable.isEmpty()
        && responseTable[0] == '#'
        && responseTable.indexOf(kNotFound) != -1;
}

template<class RuleMerger>
std::map<int, SupportedRule> mergeRules(
    const std::map<int, SupportedRule>& oldRules,
    const std::map<int, SupportedRule>& newRules,
    RuleMerger ruleMerger)
{
    std::map<int, SupportedRule> resultRules = newRules;
    for (auto& resultRule: resultRules)
    {
        auto it = oldRules.find(resultRule.first);
        if (it != oldRules.cend())
            resultRule.second = ruleMerger(it->second, resultRule.second);
    }
    return resultRules;
}

} // namespace

class CameraControllerImpl
{
    nx::network::SocketGlobals::InitGuard m_initGuard;
    nx::network::http::HttpClient m_client;
    QString m_cgiPreamble;
    static const QString kProtocol;
    static const QString kPath;

public:
    CameraControllerImpl()
    {
        // VCA-camera executes cgi-commands very slow (especially complex commands), so read
        // intervals must be really huge.
        m_client.setResponseReadTimeout(std::chrono::seconds(12));
        m_client.setMessageBodyReadTimeout(std::chrono::seconds(5));
    }

    void setCgiPreamble(const QString& ip)
    {
        m_cgiPreamble = kProtocol + ip + kPath;
    }

    void setUserPassword(const QString& user, const QString& password)
    {
        m_client.setUserName(user);
        m_client.setUserPassword(password);
    }

    void setReadTimeout(std::chrono::seconds readTimeout)
    {
        m_client.setResponseReadTimeout(readTimeout);
    }

    bool execute(const QString& command, QByteArray& report)
    {
        const QString url = m_cgiPreamble + command;

        if (!m_client.doGet(url))
            return false;

        if (!m_client.response() ||
            m_client.response()->statusLine.statusCode != nx::network::http::StatusCode::ok)
            return false;

        while (!m_client.eof())
            report.append(m_client.fetchMessageBodyBuffer());

        return true;
    }
};
const QString CameraControllerImpl::kProtocol("http://");
const QString CameraControllerImpl::kPath("/nvc-cgi/admin/param.fcgi?");

CameraController::CameraController(): m_impl(new CameraControllerImpl) {}

CameraController::CameraController(const QString& ip):
    m_ip(ip),
    m_impl(new CameraControllerImpl)
{
    m_impl->setCgiPreamble(m_ip);
}

CameraController::CameraController(const QString& ip, const QString& user, const QString& password):
    m_ip(ip),
    m_user(user),
    m_password(password),
    m_impl(new CameraControllerImpl)
{
    m_impl->setCgiPreamble(m_ip);
    m_impl->setUserPassword(m_user, m_password);
}

void CameraController::setIp(const QString& ip)
{
    m_ip = ip;
    m_impl->setCgiPreamble(m_ip);
}

void CameraController::setUserPassword(const QString& user, const QString& password)
{
    m_user = user;
    m_password = password;
    m_impl->setUserPassword(m_user, m_password);
}

void CameraController::setReadTimeout(std::chrono::seconds readTimeout)
{
    m_impl->setReadTimeout(readTimeout);
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
        return false; //< Response can't be empty, something went wrong.

    if(groupNotFound(ruleTable))
    {
        // Camera have no rules at the moment.
        m_supportedRules.clear();
        return true;
    }

    const QByteArrayList lines = splitRuleTable(ruleTable);
    std::map<int, SupportedRule> rules = parseRuleTableLines(lines);
    filterOutUnnamedRules(rules);
    if (rules.empty())
        return false; //< Parse failed.

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

    QByteArrayList lines;
    for (int i = 0; i < kGetRulesCommandCound; ++i)
    {
        QByteArray ruleTable;
        if (!m_impl->execute(kGetRulesCommand[i], ruleTable))
            continue;

        if (ruleTable.isEmpty())
            continue; //< Response can't be empty, something went wrong.

        if (groupNotFound(ruleTable))
        {
            // Camera has no rules at the moment.
            m_supportedRules.clear();
            return true;
        }

        lines += splitRuleTable(ruleTable);
    }

    std::map<int, SupportedRule> rules = parseRuleTableLines(lines);
    filterOutUnnamedRules(rules);

    if (rules.empty())
        return false;

    m_supportedRules = std::move(rules);
    return true;
}

bool CameraController::readRules(const QString& cgiCommand, std::map<int, SupportedRule>& rules)
{
    QByteArray ruleTable;
    if (!m_impl->execute(cgiCommand, ruleTable))
        return false;

    if (ruleTable.isEmpty())
        return false; //< Response can't be empty, something went wrong.

    if (groupNotFound(ruleTable))
    {
        // Camera have no rules at the moment.
        m_supportedRules.clear();
        return true;
    }

    QByteArrayList lines = splitRuleTable(ruleTable);
    rules = parseRuleTableLines(lines);

    return !rules.empty();
}

bool CameraController::readSupportedRulesState()
{
    static const QString kGetRulesCommand = "action=list&group=eventprofile.P*.enable";
    std::map<int, SupportedRule> rules;
    if (!readRules(kGetRulesCommand, rules))
        return false;

    m_supportedRules = mergeRules(m_supportedRules, rules,
        [](const SupportedRule& oldRule, const SupportedRule& newRule)
        {
            SupportedRule result = oldRule;
            result.ruleEnabled = newRule.ruleEnabled;
            return result;
        });

    return true;
}

bool CameraController::readSupportedRulesNames()
{
    static const QString kGetRulesCommand = "action=list&group=eventprofile.P*.name";
    std::map<int, SupportedRule> rules;
    if (!readRules(kGetRulesCommand, rules))
        return false;

    m_supportedRules = mergeRules(m_supportedRules, rules,
        [](const SupportedRule& oldRule, const SupportedRule& newRule)
    {
        SupportedRule result = oldRule;
        result.name = newRule.name;
        return result;
    });

    return true;
}

bool CameraController::readSupportedRulesTcpNotificationState()
{
    static const QString kGetRulesCommand =
        "action=list&group=eventprofile.P*.Notification.Tcp.enable";
    std::map<int, SupportedRule> rules;
    if (!readRules(kGetRulesCommand, rules))
        return false;

    m_supportedRules = mergeRules(m_supportedRules, rules,
        [](const SupportedRule& oldRule, const SupportedRule& newRule)
        {
            SupportedRule result = oldRule;
            result.tcpServerNotificationEnabled = newRule.tcpServerNotificationEnabled;
            return result;
        });

    return true;
}

bool CameraController::setHeartbeat(Heartbeat heartbeat) const
{
    // VCA-camera documentation sets the allowable range of heartbeat interval.
    static const std::chrono::seconds kMinInterval(1);
    static const std::chrono::seconds kMaxInterval(300);

    if (heartbeat.interval > kMaxInterval || heartbeat.interval < kMinInterval)
    {
        NX_PRINT << "Trying to set inappropriate heartbeat interval: "
            << heartbeat.interval.count() << " seconds. The value should fall within ["
            << kMinInterval.count() << ", " << kMaxInterval.count() << "]";
        return false;
    }

    const QString yesno = heartbeat.isEnabled ? "yes" : "no";
    const QString interval = QString::number(heartbeat.interval.count());
    static const QString kSetHeartbeatCommandPattern =
        "action=update&group=Event.Rule.health&tcp=yes&enable=%1&interval=%2";
    const QString setHeartbeatCommand = kSetHeartbeatCommandPattern.arg(yesno, interval);

    QByteArray cgiResponse;
    m_impl->execute(setHeartbeatCommand, cgiResponse);

    return cgiResponse.startsWith("#200");
}

QByteArray extractCgiResponseValue(const QByteArray& cgiResponse)
{
    // Correct cgiResponse is something like "Event.Notify.tcp.listenport=2555\r\n\0".
    // We need substring between '=' and '\r'.

    const int equalSignPositionPlusOne = cgiResponse.indexOf('=') + 1;
    if (equalSignPositionPlusOne == 0)
        return QByteArray(); //< Incorrect cgi response.
    const int crSignPosition = cgiResponse.indexOf('\r', equalSignPositionPlusOne);

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
    const QByteArray value = extractCgiResponseValue(cgiResponse);

    const int portNumber = value.toInt();
    if (portNumber == 0)
        return false; //< Incorrect cgi response.

    m_tcpServerPort = static_cast<int16_t>(portNumber);
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
        return false; //< Correct answer is <param>=<value>.

    const QByteArray value = extractCgiResponseValue(cgiResponse);
    const bool enabled = (value.toStdString() == "yes");
    m_supportedRules[eventProfileIndex].tcpServerNotificationEnabled = enabled;

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
    const bool result = cgiResponse.startsWith("#200");

    m_supportedRules[eventProfileIndex].tcpServerNotificationEnabled = enabled;
    return result;
}

} // namespace vca
} // namespace nx
