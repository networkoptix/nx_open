#include "camera_controller.h"

#include <QDomDocument>
#include <QMap>

#include <algorithm>

#include <nx/network/http/http_client.h>
#include <nx/network/socket_global.h>
#include <nx/kit/debug.h>

#include <nx/network/http/http_types.h>

namespace nx {
namespace dw_mtt {

namespace {

/*
 * Subscription xml consists of the beginning, several (usually up to three) items and the ending.
 * They should be just concatenated. Function "makeSubscriptionXml" does it.
 */

/*
 * The beginning of subscription xml.
 * %1 - is a parameter that should be replaced with a number of event types.
 */
const QByteArray kSubscriptionXmlBeginning = R"(
<?xml version="1.0" encoding="UTF-8"?>
<config version="1.0" xmlns="http://www.ipc.com/ver10">
    <channelID type="uint32">1</channelID>
    <initTermTime type="uint32">0</initTermTime>
    <subscribeFlag type="subscribeTypes">BASE_SUBSCRIBE</subscribeFlag>
    <subscribeList type="list" count="%1">)"; //< %1 - a number of events types

/*
 * The ending of subscription xml
 */
const QByteArray kSubscriptionXmlEnding = R"(
    </subscribeList>
</config>)";

/*
 * The following constants are the xml elements for diffrerent kind of events in the
 * subscription request. Each of them has one of the thee options:
 * ALARM - SendAlarmStatus is sent in case of event
 * FEATURE_RESULT - SendAlarmData is sent in case of event
 * ALARM_FEATURE - both SendAlarmStatus and SendAlarmData are sent in case of event
 *
 * The current version of plugin uses ALARM option, i.e. SendAlarmStatus notification.
*/

/*
 * MOTION - motion detection.
 * The event can be combined with any other event.
 * Name in web interface: "Alarm/Motion Detection".
 * Description in web interface: no description.
 * Detection area: 22x15 matrix.
 */
const QByteArray kSubscriptionXmlMotionItem = R"(
        <item>
            <smartType type="smartType">MOTION</smartType>
            <subscribeRelation type="subscribeOption">ALARM</subscribeRelation>
        </item>)";

/*
 * VFD - video face detection
 * The event cannot be combined with CDD, CPC, IPD, PEA or OSC event.
 * Name in web interface: "Face Detection".
 * Description in web interface: "Smart detection of faces appeared in the tracked scene".
 * Detection area: rectangle.
 * Currentrly (march 2018) VFD is not supported by DW MTT Camera.
 */
const QByteArray kSubscriptionXmlVfdItem = R"(
        <item>
            <smartType type="smartType">VFD</smartType>
            <subscribeRelation type="subscribeOption">ALARM</subscribeRelation>
        </item>)";

/*
 * CDD - crowd density detection
 * The event cannot be combined with VFD, CPC, IPD, PEA or OSC event.
 * Name in web interface: "Crowd Density"
 * Description in web interfac: "Detect the crowd density in certain area".
 * Detection area: rectangle.
 */
const QByteArray kSubscriptionXmlCddItem = R"(
        <item>
            <smartType type="smartType">CDD</smartType>
            <subscribeRelation type="subscribeOption">ALARM</subscribeRelation>
        </item>)";

/*
 * CPC - cross-line people counting
 * The event cannot be combined with VFD, CDD, IPD, PEA or OSC event.
 * Name in web interface: "People Couning"
 * Description in web interface: "Detect and calculate the entrancing and departing people
 * in certain area".
 * Detection area: rectangle. Also The direction if people movement is set.
 */
const QByteArray kSubscriptionXmlCpcItem = R"(
        <item>
            <smartType type="smartType">CPC</smartType>
            <subscribeRelation type="subscribeOption">ALARM</subscribeRelation>
        </item>)";

/*
 * IPD - intruding people detection
 * The event cannot be combined with VFD, CDD, CPC, PEA or OSC event.
 * Name in web interface: "People Intrusion"
 * Description in web interface: "Detect the intrusion people in a closed area".
 * Detection Area: whole frame (no settings in web interface)
 */
const QByteArray kSubscriptionXmlIpdItem = R"(
        <item>
            <smartType type="smartType">IPD</smartType>
            <subscribeRelation type="subscribeOption">ALARM</subscribeRelation>
        </item>)";

/*
 * PEA - some abbriviation, if you know - write it here
 * The event cannot be combined with VFD, CDD, CPC, IPD or OSC event.
 * This event detects crossing of some border (this is similar to IPD and CPC events).
 * The border may be a line or a polygon (6 vertices max).
 * In web interface this event corresponds to two separate user events:
 * "Line Crossing" and "Intrusion" (only one can be active at a time).
 * First is described as "Smart area crossing analasys", second - as "Smart intrusion detection".
 * Detection Area: for Line Crossing - line segment, for Intrusion - polygon (6 vertices max).
 * The main differences from IPD and CPC is that they people, and PEA - any objects.
 * Also detection areas are set differently (e.g. IPD's area is always a whole frame).
 */
const QByteArray kSubscriptionXmlPeaItem = R"(
        <item>
            <smartType type="smartType">PEA</smartType>
            <subscribeRelation type="subscribeOption">ALARM</subscribeRelation>
        </item>)";

/*
 * OSC - object status changed
 * The event cannot be combined with VFD, CDD, CPC, IPD or PEA event.
 * Name in web interface: "Object Removal"
 * Description in web interface: "Smart detection of left, lost or moved items".
 * Detection Area: polygon (6 vertices max)
 */
const QByteArray kSubscriptionXmlOscItem = R"(
        <item>
            <smartType type="smartType">OSC</smartType>
            <subscribeRelation type="subscribeOption">ALARM</subscribeRelation>
        </item>)";

/*
 * AVD - abnormal video diagnostic
 * The event can be combined with any other event.
 * Name in web interface: "Exception"
 * Description in web interface: "Correction of image distortion" - which is confusing.
 * Detection Area: whole frame (no settings in web interface)
 * AVD may detect one of the three events (names are taken from web interface):
 * "Scene change detection", "Video blur detection" and "Video cast detection", which are
 * described as "Scene change", "Abnormal clarity" and "Color abnormal".
 */
const QByteArray kSubscriptionXmlAvdItem = R"(
        <item>
            <smartType type="smartType">AVD</smartType>
            <subscribeRelation type="subscribeOption">ALARM</subscribeRelation>
        </item>)";

/*
 * Map allows to get subscrition xml item for the event by its internal name.
 */
const QMap<QByteArray, QByteArray> kXmlItemsByInternalName =
{
    {"MOTION", kSubscriptionXmlMotionItem},
    {"VFD", kSubscriptionXmlVfdItem},
    {"CDD", kSubscriptionXmlCddItem},
    {"CPC", kSubscriptionXmlCpcItem},
    {"IPD", kSubscriptionXmlIpdItem},
    {"PEA", kSubscriptionXmlPeaItem},
    {"OSC", kSubscriptionXmlOscItem},
    {"AVD", kSubscriptionXmlAvdItem},
};

/*
 * DW MTT camera is sensitive to endlines in xml int http body. Only '\n' is appropriate.
 * Also no endlines and spaces are allowed in the begin and in the end.
 */
QByteArray normalizeXmlEndlines(const QByteArray& source)
{

    const QByteArray kSystemEndline = R"(
)"; //< This line consists of endline marker (i.e. "\n" or "\r\n" or any other, it doesnt' matter.)

    const QByteArray kDesiredEndline = "\n";
    QByteArray result = source;
    result.replace(kSystemEndline, kDesiredEndline);
    result = result.trimmed();
    return result;
}

} // namespace

/*
 * Make xml body for subscruption request. Concatenates the beginning, xml items from events and
 * the ending. The resulting xml is normalized (endlines are set to '\n'). Xml items are searched
 * in kXmlItemsByInternalName map.
 */
QByteArray makeSubscriptionXml(const QSet<QByteArray>& eventInternalNames)
{
    // First, check the names.
    int checkedEventsCount = 0;
    for (const auto& name: eventInternalNames)
    {
        if (kXmlItemsByInternalName.contains(name))
            ++checkedEventsCount;
        else
            NX_PRINT << "Unreconginzed event name: " << name.constData();
    }

    if (checkedEventsCount == 0)
    {
        NX_PRINT << "Mo events no subscribe to.";
        return QByteArray();
    }

    QByteArray result = kSubscriptionXmlBeginning;

    static const QByteArray kItemsCountMarker("%1");

    int markerIndex = result.lastIndexOf(kItemsCountMarker);
    if (markerIndex != -1)
        result.replace(markerIndex, kItemsCountMarker.size(), QByteArray::number(checkedEventsCount));

    for (const auto& item: eventInternalNames)
        result += kXmlItemsByInternalName.value(item, "");

    result += kSubscriptionXmlEnding;
    result = normalizeXmlEndlines(result);
    return result;
}

class CameraControllerImpl
{
    nx::network::SocketGlobals::InitGuard m_initGuard;
    nx::network::http::HttpClient m_client;
    QByteArray m_cgiPreamble;
    static const QByteArray kProtocol;
    static const QByteArray kPath;

public:
    CameraControllerImpl()
    {
        m_client.setResponseReadTimeout(std::chrono::seconds(5));
        m_client.setMessageBodyReadTimeout(std::chrono::seconds(5));
    }

    void setCgiPreamble(const QByteArray& ip)
    {
        m_cgiPreamble = kProtocol + ip + kPath;
    }

    void setCredentials(const QByteArray& user, const QByteArray& password)
    {
        m_client.setUserName(user);
        m_client.setUserPassword(password);
    }

    void setReadTimeout(std::chrono::seconds readTimeout)
    {
        m_client.setResponseReadTimeout(readTimeout);
    }

    /*
     * Execute command (e.g. GET http://10.0.0.1/bla-bla-bla). If reply in error occures retruns
     * false, otherwiae - true. If repry is not null the boby of pesponse is copied into it.
     */
    bool execute(const QByteArray& command, QByteArray* reply)
    {
        const QByteArray url = m_cgiPreamble + command;
        if (!m_client.doGet(QString::fromLatin1(url)))
            return false;

        if (!m_client.response() ||
            m_client.response()->statusLine.statusCode != nx::network::http::StatusCode::ok)
            return false;

        auto optionalBody = m_client.fetchEntireMessageBody();

        if (!optionalBody)
            return false;

        if (reply)
            *reply = *optionalBody;

        return true;
    }
};
const QByteArray CameraControllerImpl::kProtocol("http://");
const QByteArray CameraControllerImpl::kPath("/");

QDomElement findChildTag(QDomElement parent, QString tagName)
{
    for (auto port = parent.firstChild(); !port.isNull(); port = port.nextSibling())
    {
        QDomElement tag = port.toElement();
        if (!tag.isNull() && tag.tagName() == tagName)
            return tag;
    }
    return QDomElement();
}

int16_t readPort(const QDomElement& parent, const QString& name)
{
    QDomElement port = findChildTag(parent, name);
    if (port.isNull())
        return 0;
    else
        return static_cast<uint16_t>(port.text().toInt());
}

CameraController::CameraController() : m_impl(new CameraControllerImpl) {}

CameraController::CameraController(const QByteArray& ip) :
    m_ip(ip),
    m_impl(new CameraControllerImpl)
{
    m_impl->setCgiPreamble(m_ip);
}

CameraController::CameraController(const QByteArray& ip,
    const QByteArray& user, const QByteArray& password):
    m_ip(ip),
    m_user(user),
    m_password(password),
    m_impl(new CameraControllerImpl)
{
    m_impl->setCgiPreamble(m_ip);
    m_impl->setCredentials(m_user, m_password);
}

void CameraController::setIp(const QByteArray& ip)
{
    m_ip = ip;
    m_impl->setCgiPreamble(m_ip);
}

void CameraController::setCredentials(const QByteArray& user, const QByteArray& password)
{
    m_user = user;
    m_password = password;
    m_impl->setCredentials(m_user, m_password);
}

void CameraController::setReadTimeout(std::chrono::seconds readTimeout)
{
    m_impl->setReadTimeout(readTimeout);
}

bool CameraController::readPortConfiguration()
{
    static const QByteArray kGetPortConfigCommand = "GetPortConfig";

    static const QString kRootTagName = "config";
    static const QString kPortTagName = "port";

    static const QString kPortNames[] =
        {"httpPort", "netPort", "rtspPort", "httpsPort", "longPollingPort"};

    uint16_t* portNumbers[] =
        {&m_httpPort, &m_netPort, &m_rtspPort, &m_httpsPort, &m_longPollingPort};

    QByteArray xml;
    if (!m_impl->execute(kGetPortConfigCommand, &xml))
        return false;

    if (xml.isEmpty())
        return false; //< Response can't be empty, something went wrong.

    std::map<QString, QString> portMap;
    QDomDocument dom;
    dom.setContent(xml);
    QDomElement root = dom.documentElement();
    if (root.tagName() != kRootTagName)
        return false;

    QDomElement ports = findChildTag(root, kPortTagName);
    if (ports.isNull())
        return false;

    for(int i=0; i < 5; ++i)
         *portNumbers[i] = readPort(ports, kPortNames[i]);

    return true;
}

nx::network::http::Request CameraController::makeHttpRequest(const QByteArray& body)
{
    nx::network::http::RequestLine requestLine{ "POST", nx::utils::Url("/SetSubscribe"), { "HTTP", "1.1" } };

    nx::network::http::header::BasicAuthorization basic(m_user, m_password);
    QByteArray bodyLength = QByteArray::number(body.size());

    nx::network::http::HttpHeaders headers =
    {
        { nx::network::http::header::Authorization::NAME, basic.serialized() },
        { "User-Agent", "ApiTool" },
        { "Host", m_ip },
        { "Content-Length", bodyLength },
    };

    return nx::network::http::Request{ requestLine, headers, body };
}

} // namespace dw_mtt
} // namespace nx
