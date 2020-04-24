#include "camera_controller.h"

#include <QDomDocument>
#include <QMap>

#include <algorithm>

#include <nx/network/http/http_client.h>
#include <nx/network/socket_global.h>
#include <nx/kit/debug.h>

#include <nx/network/http/http_types.h>

namespace nx::dw_tvt {

namespace {

/** Makes proper xml: no indentations, no empty lines, correct endlines. */
QByteArray operator""_proper_xml(const char* data, size_t size)
{
    QByteArray xml(data, size);
    normalizeEndlines(&xml);
    unindentLines(&xml);
    return xml.trimmed();
}

/**
 @defgroup subscriptionXml Subscription XML

 * Subscription xml consists of the beginning, several (usually up to three) items and the ending.
 * They should be just concatenated. Function "buildSubscriptionXml" does it.
 *
 * kSubscriptionXmlXXXXXItem constants are the xml elements for different kind of events in the
 * subscription request. Each of them has one of the thee options:
 * ALARM - SendAlarmStatus is sent in case of event
 * FEATURE_RESULT - SendAlarmData is sent in case of event
 * ALARM_FEATURE - both SendAlarmStatus and SendAlarmData are sent in case of event
 * The current version of plugin uses ALARM_FEATURE option
 */

 /** @{ */

/**
 * The pattern of the subscription xml.
 * %1 - should be replaced with a number of event types.
 * %2 - should be replaced with event type items.
 */
static QByteArray kSubscriptionXmlPattern = R"(
<?xml version="1.0" encoding="UTF-8"?>
<config version="1.0" xmlns="http://www.ipc.com/ver10">
    <types>
        <openAlramObj>
            <enum>MOTION</enum>
            <enum>SENSOR</enum>
            <enum>PEA</enum>
            <enum>AVD</enum>
            <enum>OSC</enum>
            <enum>CPC</enum>
            <enum>CDD</enum>
            <enum>IPD</enum>
            <enum>VFD</enum>
            <enum>VFD_MATCH</enum>
            <enum>VEHICE</enum>
            <enum>AOIENTRY</enum>
            <enum>AOILEAVE</enum>
            <enum>PASSLINECOUNT</enum>
            <enum>TRAFFIC</enum>
        </openAlramObj>
        <subscribeRelation>
            <enum>ALARM</enum>
            <enum>FEATURE_RESULT</enum>
            <enum>ALARM_FEATURE</enum>
        </subscribeRelation>
        <subscribeTypes>
            <enum>BASE_SUBSCRIBE</enum>
            <enum>REALTIME_SUBSCRIBE</enum>
            <enum>STREAM_SUBSCRIBE</enum>
        </subscribeTypes>
    </types>
    <channelID type="uint32">0</channelID>
    <initTermTime type="uint32">0</initTermTime>
    <subscribeFlag type="subscribeTypes">BASE_SUBSCRIBE</subscribeFlag>
    <subscribeList type="list" count="%1">
        %2
    </subscribeList>
</config>
)"_proper_xml;

/**
 * MOTION - motion detection.
 * The event can be combined with any other event.
 * Name in web interface: "Alarm/Motion Detection".
 * Description in web interface: no description.
 * Detection area: 22x15 matrix.
 */
static const QByteArray kSubscriptionXmlMotionItem = R"(
<item>
    <smartType type="openAlramObj">MOTION</smartType>
    <subscribeRelation type="subscribeRelation">ALARM_FEATURE</subscribeRelation>
</item>
)"_proper_xml;

/**
 * AVD - abnormal video diagnostic
 * The event can be combined with any other event.
 * Name in web interface: "Exception"
 * Description in web interface: "Correction of image distortion" - which is confusing.
 * Detection Area: whole frame (no settings in web interface)
 * AVD may detect one of the three events (names are taken from web interface):
 * "Scene change detection", "Video blur detection" and "Video cast detection", which are
 * described as "Scene change", "Abnormal clarity" and "Color abnormal".
 */
static const QByteArray kSubscriptionXmlAvdItem = R"(
<item>
    <smartType type="openAlramObj">AVD</smartType>
    <subscribeRelation type="subscribeRelation">ALARM_FEATURE</subscribeRelation>
</item>
)"_proper_xml;

/**
 * PEA - some abbreviation, if you know - write it here
 * The event cannot be combined with VFD, CDD, CPC, IPD or OSC event.
 * This event detects crossing of some border (this is similar to IPD and CPC events).
 * The border may be a line or a polygon (6 vertices max).
 * In web interface this event corresponds to two separate user events:
 * "Line Crossing" and "Intrusion" (only one can be active at a time).
 * First is described as "Smart area crossing analysis", second - as "Smart intrusion detection".
 * Detection Area: for Line Crossing - line segment, for Intrusion - polygon (6 vertices max).
 * The main differences from IPD and CPC is that they people, and PEA - any objects.
 * Also detection areas are set differently (e.g. IPD's area is always a whole frame).
 */
static const QByteArray kSubscriptionXmlPeaItem = R"(
<item>
    <smartType type="openAlramObj">PEA</smartType>
    <subscribeRelation type="subscribeRelation">ALARM_FEATURE</subscribeRelation>
</item>
)"_proper_xml;

/**
 * VFD - video face detection
 * The event cannot be combined with CDD, CPC, IPD, PEA or OSC event.
 * Name in web interface: "Face Detection".
 * Description in web interface: "Smart detection of faces appeared in the tracked scene".
 * Detection area: rectangle.
 * Currently (march 2020) VFD is not supported by DW TVT Camera.
 */
static const QByteArray kSubscriptionXmlVfdItem = R"(
<item>
    <smartType type="openAlramObj">VFD</smartType>
    <subscribeRelation type="subscribeRelation">ALARM_FEATURE</subscribeRelation>
</item>
)"_proper_xml;

/**
 * CDD - crowd density detection
 * The event cannot be combined with VFD, CPC, IPD, PEA or OSC event.
 * Name in web interface: "Crowd Density"
 * Description in web interface: "Detect the crowd density in certain area".
 * Detection area: rectangle.
 */
static const QByteArray kSubscriptionXmlCddItem = R"(
<item>
    <smartType type="openAlramObj">CDD</smartType>
    <subscribeRelation type="subscribeRelation">ALARM_FEATURE</subscribeRelation>
</item>
)"_proper_xml;

/**
 * CPC - cross-line people counting
 * The event cannot be combined with VFD, CDD, IPD, PEA or OSC event.
 * Name in web interface: "People Counting"
 * Description in web interface: "Detect and calculate the entrancing and departing people
 * in certain area".
 * Detection area: rectangle. Also The direction if people movement is set.
 */
static const QByteArray kSubscriptionXmlCpcItem = R"(
<item>
    <smartType type="openAlramObj">CPC</smartType>
    <subscribeRelation type="subscribeRelation">ALARM_FEATURE</subscribeRelation>
</item>
)"_proper_xml;

/**
 * IPD - intruding people detection
 * The event cannot be combined with VFD, CDD, CPC, PEA or OSC event.
 * Name in web interface: "People Intrusion"
 * Description in web interface: "Detect the intrusion people in a closed area".
 * Detection Area: whole frame (no settings in web interface)
 */
static const QByteArray kSubscriptionXmlIpdItem = R"(
<item>
    <smartType type="openAlramObj">IPD</smartType>
    <subscribeRelation type="subscribeRelation">ALARM_FEATURE</subscribeRelation>
</item>
)"_proper_xml;

/**
 * OSC - object status changed
 * The event cannot be combined with VFD, CDD, CPC, IPD or PEA event.
 * Name in web interface: "Object Removal"
 * Description in web interface: "Smart detection of left, lost or moved items".
 * Detection Area: polygon (6 vertices max)
 */
static const QByteArray kSubscriptionXmlOscItem = R"(
<item>
    <smartType type="openAlramObj">OSC</smartType>
    <subscribeRelation type="subscribeRelation">ALARM_FEATURE</subscribeRelation>
</item>
)"_proper_xml;

/** @} */

// New (March, 2020) events

static const QByteArray kSubscriptionXmlAoientryItem = R"(
<item>
    <smartType type="openAlramObj">AOIENTRY</smartType>
    <subscribeRelation type="subscribeRelation">ALARM_FEATURE</subscribeRelation>
</item>
)"_proper_xml;

static const QByteArray kSubscriptionXmlAoileaveItem = R"(
<item>
    <smartType type="openAlramObj">AOILEAVE</smartType>
    <subscribeRelation type="subscribeRelation">ALARM_FEATURE</subscribeRelation>
</item>
)"_proper_xml;

static const QByteArray kSubscriptionXmlPasslinecountItem = R"(
<item>
    <smartType type="openAlramObj">PASSLINECOUNT</smartType>
    <subscribeRelation type="subscribeRelation">ALARM_FEATURE</subscribeRelation>
</item>
)"_proper_xml;

static const QByteArray kSubscriptionXmlTrafficItem = R"(
<item>
    <smartType type="openAlramObj">TRAFFIC</smartType>
    <subscribeRelation type="subscribeRelation">ALARM_FEATURE</subscribeRelation>
</item>
)"_proper_xml;

/** Map allows to get subscription xml item for the event by its internal name. */
static const QMap<QByteArray, QByteArray> kXmlItemsByInternalName =
{
    { "MOTION", kSubscriptionXmlMotionItem },
    { "AVD", kSubscriptionXmlAvdItem },
    { "PEA", kSubscriptionXmlPeaItem },

    { "VFD", kSubscriptionXmlVfdItem }, //< mot supported in fw 5.0 (April 2020)
    { "CDD", kSubscriptionXmlCddItem }, //< mot supported in fw 5.0 (April 2020)
    { "CPC", kSubscriptionXmlCpcItem }, //< mot supported in fw 5.0 (April 2020)
    { "IPD", kSubscriptionXmlIpdItem }, //< mot supported in fw 5.0 (April 2020)
    { "OSC", kSubscriptionXmlOscItem }, //< mot supported in fw 5.0 (April 2020)

    { "AOIENTRY", kSubscriptionXmlAoientryItem },
    { "AOILEAVE", kSubscriptionXmlAoileaveItem },
    { "PASSLINECOUNT", kSubscriptionXmlPasslinecountItem },

    { "TRAFFIC", kSubscriptionXmlTrafficItem }, //< mot supported in fw 5.0 (April 2020)
};

} // namespace

void normalizeEndlines(QByteArray* xml)
{
    const QByteArray kSystemEndline = R"(
)"; //< This line consists of endline marker (i.e. "\n" or "\r\n" or any other, it doesn't matter).

    const QByteArray kDesiredEndline = "\n";
    xml->replace(kSystemEndline, kDesiredEndline);
}

void unindentLines(QByteArray* xml)
{
    QByteArray result;
    result.reserve(xml->size());

    bool newLine = true;
    for (const char c : *xml)
    {
        if (newLine)
        {
            if (std::isspace(c))
                continue;
            else
                newLine = false;
        }

        result.push_back(c);

        if (c == '\n')
            newLine = true;
    }
    *xml = result;
}

QByteArray buildSubscriptionXml(const QSet<QByteArray>& eventInternalNames)
{
    // First, check the names.
    int checkedEventsCount = 0;
    for (const auto& name: eventInternalNames)
    {
        if (kXmlItemsByInternalName.contains(name))
            ++checkedEventsCount;
        else
            NX_PRINT << "Unrecognized event name: " << name.constData();
    }

    if (checkedEventsCount == 0)
    {
        NX_PRINT << "No events to subscribe to.";
        return QByteArray();
    }

    QByteArray xmlCore;
    for (const auto& item: eventInternalNames)
        xmlCore += kXmlItemsByInternalName.value(item, "");

    QByteArray result = kSubscriptionXmlPattern;
    result.replace("%1", QByteArray::number(checkedEventsCount)).replace("%2", xmlCore);

    return result;
}

//-------------------------------------------------------------------------------------------------

class CameraHttpClient
{
    nx::network::SocketGlobals::InitGuard m_initGuard;
    nx::network::http::HttpClient m_client;
    QByteArray m_cgiPreamble;
    static const QByteArray kProtocol;
    static const QByteArray kPath;

public:
    CameraHttpClient()
    {
        m_client.setResponseReadTimeout(std::chrono::seconds(5));
        m_client.setMessageBodyReadTimeout(std::chrono::seconds(5));
    }

    void setCgiPreamble(const QByteArray& ip, unsigned short port = 0)
    {
        QByteArray portAsString = port ? (":" + QByteArray::number(port)) : QByteArray();
        m_cgiPreamble = kProtocol + ip + portAsString + kPath;
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

    /**
     * Execute command (e.g. GET http://10.0.0.1/bla-bla-bla).
     * If `reply` is not null the body of response is copied into it.
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
const QByteArray CameraHttpClient::kProtocol("http://");
const QByteArray CameraHttpClient::kPath("/");

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

CameraController::CameraController(): m_cameraHttpClient(new CameraHttpClient) {}

CameraController::CameraController(const QByteArray& ip, unsigned short port):
    CameraController()
{
    m_ip = ip;
    m_port = port;
    m_cameraHttpClient->setCgiPreamble(m_ip, m_port);
}

CameraController::CameraController(const QByteArray& ip, unsigned short port,
    const QByteArray& user, const QByteArray& password)
    :
    CameraController(ip, port)
{
    m_cameraHttpClient->setCredentials(m_user, m_password);
}

CameraController::~CameraController() = default;

void CameraController::setIpPort(const QByteArray& ip, unsigned short port)
{
    m_ip = ip;
    m_port = port;
    m_cameraHttpClient->setCgiPreamble(m_ip, m_port);
}

void CameraController::setCredentials(const QByteArray& user, const QByteArray& password)
{
    m_user = user;
    m_password = password;
    m_cameraHttpClient->setCredentials(m_user, m_password);
}

void CameraController::setReadTimeout(std::chrono::seconds readTimeout)
{
    m_cameraHttpClient->setReadTimeout(readTimeout);
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
    if (!m_cameraHttpClient->execute(kGetPortConfigCommand, &xml))
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

} // namespace nx::dw_tvt
