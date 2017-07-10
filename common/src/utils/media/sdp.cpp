#include "sdp.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace sdp {

namespace {

const static QString kVersionPrefix = lit("v=");
const static QString kOriginPrefix = lit("o=");
const static QString kSessionNamePrefix = lit("s=");
const static QString kSessionInformationPrefix = lit("i=");
const static QString kUriPrefix = lit("u=");
const static QString kEmailPrefix = lit("e=");
const static QString kPhonePrefix = lit("p=");
const static QString kConnectionInformationPrefix = lit("c=");
const static QString kBandwidthPrefix = lit("b=");
const static QString kTimingPrefix = lit("t=");
const static QString kRepeatTimesPrefix = lit("r=");
const static QString kMediaDescriptionPrefix = lit("m=");
const static QString kEncryptionKeyPrefix = lit("k=");
const static QString kTimeZonePrefix = lit("z=");
const static QString kAttributePrefix = lit("a=");
const static QString kMediaTitlePrefix = lit("i=");

const static int kPrefixSize = 2;
const static int64_t kNtpUnixEpochDifferenceSeconds(2208988800);

const static int kSecondsInMinute(60);
const static int kSecondsInHour(3600);
const static int kSecondsInDay(86400);

const static QString kDayPostfix = lit("d");
const static QString kHourPostfix = lit("h");
const static QString kMinutePostfix = lit("m");
const static QString kSecondPostfix = lit("s");

template<typename ElementType>
void removeElementFromVector(std::vector<ElementType>* vec, ElementType element)
{
    auto itr = std::find(vec->begin(), vec->end(), element);
    if (itr != vec->end())
        vec->erase(itr);
}

} // namespace

Timing TimeDescription::timing() const
{
    return m_timing;
}

std::vector<RepeatTimes> TimeDescription::repeatTimes() const
{
    return m_repeatTimes;
}

void TimeDescription::setTiming(const Timing& timing)
{
    m_timing = timing;
}

void TimeDescription::addRepeatTimes(const RepeatTimes& repeatTimes)
{
    m_repeatTimes.push_back(repeatTimes);
}

void TimeDescription::removeRepeatTimes(const RepeatTimes& repeatTimes)
{
    removeElementFromVector(&m_repeatTimes, repeatTimes);
}

bool TimeDescription::operator==(const TimeDescription& other)
{
    return m_timing == other.m_timing
        && m_repeatTimes == other.m_repeatTimes;
}

//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------

MediaType MediaDescription::mediaType() const
{
    return m_mediaParams.mediaType;
}

int MediaDescription::port() const
{
    return m_mediaParams.port;
}

int MediaDescription::numberOfPorts() const
{
    return m_mediaParams.numberOfPorts;
}

QString MediaDescription::format() const
{
    return m_mediaParams.format;
}

MediaParams MediaDescription::mediaParams() const
{
    return m_mediaParams;
}

boost::optional<QString> MediaDescription::mediaTitle() const
{
    return m_mediaTitle;
}

boost::optional<ConnectionInfo> MediaDescription::connectionInfo() const
{
    return m_connectionInfo;
}

boost::optional<Bandwidth> MediaDescription::bandwidth() const
{
    return m_bandwidth;
}

boost::optional<QString> MediaDescription::attribute(const QString& attributeName) const
{
    auto itr = m_attributes.find(attributeName);
    if (itr != m_attributes.end())
        return itr->second;

    return boost::none;
}

std::map<QString, QString> MediaDescription::attributes() const
{
    return m_attributes;
}

void MediaDescription::setMediaParams(const MediaParams& mediaParams)
{
    m_mediaParams = mediaParams;
}

void MediaDescription::setMediaType(MediaType mediaType)
{
    m_mediaParams.mediaType = mediaType;
}

void MediaDescription::setPort(int port)
{
    m_mediaParams.port = port;
}

void MediaDescription::setNumberOfPorts(int numberOfPorts)
{
    m_mediaParams.numberOfPorts = numberOfPorts;
}

void MediaDescription::setFormat(const QString& format)
{
    m_mediaParams.format = format;
}

void MediaDescription::setMediaTitle(const boost::optional<QString>& mediaTitle)
{
    m_mediaTitle = mediaTitle;
}

void MediaDescription::setConnectionInfo(const boost::optional<ConnectionInfo>& connectionInfo)
{
    m_connectionInfo = connectionInfo;
}

void MediaDescription::setBandwidth(const boost::optional<Bandwidth>& bandwidth)
{
    m_bandwidth = bandwidth;
}

void MediaDescription::setAttribute(const QString& attributeName, const QString& attributeValue)
{
    m_attributes[attributeName] = attributeValue;
}

void MediaDescription::removeAttribute(const QString& attributeName)
{
    m_attributes.erase(attributeName);
}

bool MediaDescription::operator==(const MediaDescription& other)
{
    return m_mediaParams == other.m_mediaParams
        && m_mediaTitle == other.m_mediaTitle
        && m_connectionInfo == other.m_connectionInfo
        && m_bandwidth == other.m_bandwidth
        && m_attributes == other.m_attributes;
}

//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------

bool Description::decode(QByteArray& rawData)
{
    auto lines = rawData.split('\r');
    return parseSessionLines(lines.begin(), lines.end());
}

bool Description::decode(QByteArrayList& rawData)
{
    return parseSessionLines(rawData.begin(), rawData.end());
}

QByteArray Description::encode()
{
    // TODO: #dmihin implement;
    NX_ASSERT(false, "Is not implemented yet!");
    return QByteArray();
}

QString Description::version() const
{
    return m_version;
}

QString Description::origin() const
{
    return m_origin;
}

QString Description::sessionName() const
{
    return m_sessionName;
}

boost::optional<QString> Description::sessionInformation() const
{
    return m_sessionInformation;
}

boost::optional<QUrl> Description::uri() const
{
    return m_url;
}

boost::optional<QString> Description::email() const
{
    return m_email;
}

boost::optional<QString> Description::phone() const
{
    return m_phone;
}

boost::optional<Bandwidth> Description::bandwidth() const
{
    return m_bandwidth;
}

boost::optional<ConnectionInfo> Description::connectionInfo() const
{
    return m_connectionInfo;
}

std::vector<TimeDescription> Description::timeDescriptions() const
{
    return m_timeDescriptions;
}

std::vector<TimeZone> Description::timeZones() const
{
    return m_timeZones;
}

std::map<QString, QString> Description::attributes() const
{
    return m_attributes;
}

boost::optional<QString> Description::attribute(const QString& attributeName) const
{
    auto itr = m_attributes.find(attributeName);
    if (itr != m_attributes.end())
        return itr->second;

    return boost::none;
}

std::vector<MediaDescription> Description::mediaDescriptions() const
{
    return m_mediaDescriptions;
}

void Description::setVersion(const QString& version)
{
    m_version = version;
}

void Description::setOrigin(const QString& origin)
{
    m_origin = origin;
}

void Description::setSessionName(const QString& sessionName)
{
    m_sessionName = sessionName;
}

void Description::setSessionInformation(const boost::optional<QString>& sessionInformation)
{
    m_sessionInformation = sessionInformation;
}

void Description::setUri(const boost::optional<QUrl>& url)
{
    m_url = url;
}

void Description::setEmail(const boost::optional<QString>& email)
{
    m_email = email;
}

void Description::setPhone(const boost::optional<QString>& phone)
{
    m_phone = phone;
}

void Description::setBandwidth(const boost::optional<Bandwidth>& bandwidth)
{
    m_bandwidth = bandwidth;
}

void Description::setConnectionInfo(const boost::optional<ConnectionInfo>& connectionInfo)
{
    m_connectionInfo = connectionInfo;
}

void Description::addTimeDescription(const TimeDescription& timeDescription)
{
    m_timeDescriptions.push_back(timeDescription);
}

void Description::removeTimeDescription(const TimeDescription& timeDescription)
{
    removeElementFromVector(&m_timeDescriptions, timeDescription);
}

void Description::addTimezone(const TimeZone& timeZone)
{
    m_timeZones.push_back(timeZone);
}

void Description::removeTimezone(const TimeZone& timeZone)
{
    removeElementFromVector(&m_timeZones, timeZone);
}

void Description::addAttribute(const QString& attributeName, const QString& attributeValue)
{
    m_attributes[attributeName] = attributeValue;
}

void Description::removeAttribute(const QString& attributeName)
{
    m_attributes.erase(attributeName);
}

void Description::addMediaDescription(const MediaDescription& mediaDescription)
{
    m_mediaDescriptions.push_back(mediaDescription);
}

void Description::removeMediaDescription(const MediaDescription& mediaDescription)
{
    removeElementFromVector(&m_mediaDescriptions, mediaDescription);
}

bool Description::isSimpleStringLine(const QByteArray& line) const
{
    return line.startsWith(kVersionPrefix.toUtf8())
        || line.startsWith(kOriginPrefix.toUtf8())
        || line.startsWith(kSessionNamePrefix.toUtf8())
        || line.startsWith(kSessionInformationPrefix.toUtf8())
        || line.startsWith(kEmailPrefix.toUtf8())
        || line.startsWith(kPhonePrefix.toUtf8());
}

std::pair<QString, QString> Description::parseSimpleStringLine(const QByteArray& line) const
{
    auto prefix = line.left(kPrefixSize);
    auto value = line.mid(kPrefixSize);

    return std::pair<QString, QString>(
        QString::fromUtf8(prefix),
        QString::fromUtf8(value));
}

QUrl Description::parseUriLine(const QByteArray& line) const
{
    return QUrl(QString::fromUtf8(line.mid(kPrefixSize)));
}

ConnectionInfo Description::parseConnectionInfoLine(const QByteArray& line) const
{
    auto value = line.mid(kPrefixSize);
    auto split = value.split(L' ');

    if (split.size() != 3)
        return ConnectionInfo();

    ConnectionInfo connectionInfo;
    connectionInfo.networkType = ConnectionInfo::fromStringToNetworkType(
        QString::fromUtf8(split[0].trimmed()));
    connectionInfo.addressType = ConnectionInfo::fromStringToAddressType(
        QString::fromUtf8(split[1].trimmed()));

    auto params = split[2].split(L'/');
    
    connectionInfo.address = HostAddress(QString::fromUtf8(params[0]));

    if (params.size() >= 2)
        connectionInfo.ttl = params[1].toInt();

    if (params.size() >= 3)
        connectionInfo.numberOfAddresses = params[2].toInt();

    return connectionInfo;
}

Bandwidth Description::parseBandwidthLine(const QByteArray& line) const
{
    auto value = line.mid(kPrefixSize);
    auto split = value.split(L':');
    if (split.size() != 2)
        return Bandwidth();

    Bandwidth bandwidth;
    bandwidth.bandwidthType = Bandwidth::fromStringToBandwidthType(
        QString::fromUtf8(split[0]));
    bandwidth.bandwidthKbitPerSecond = split[1].toInt();

    return bandwidth;
}

std::pair<QString, QString> Description::parseAttributeLine(const QByteArray& line) const
{
    auto value = line.mid(kPrefixSize);
    auto delimiter = value.indexOf(L':');

    if(delimiter)
    {
        return std::pair<QString, QString>(
            QString::fromUtf8(value.left(delimiter)),
            QString::fromUtf8(value.mid(delimiter + 1)));
    }

    return std::pair<QString, QString>(
        QString::fromUtf8(value),
        QString());
}

std::vector<TimeZone> Description::parseTimeZoneLine(const QByteArray& line) const
{
    auto split = line.mid(kPrefixSize).split(L' ');

    if (split.size() % 2 != 0)
        return std::vector<TimeZone>();

    std::vector<TimeZone> timeZones;
    for (auto i = 0; i < split.size(); i += 2)
    {
        TimeZone timeZone;
        timeZone.adjustmentTimeSeconds = parseTimeString(QString::fromUtf8(split[i]));
        timeZone.offsetSeconds = parseTimeString(QString::fromUtf8(split[i + 1]));
        timeZones.push_back(timeZone);
    }

    return timeZones;
}

Timing Description::parseTimingLine(const QByteArray& line) const
{
    auto split = line.mid(kPrefixSize).split(L' ');

    if (split.size() != 2)
        return Timing();

    Timing timing;
    timing.startTimeSecondsSinceEpoch = split[0].toLongLong();
    timing.endTimeSecondsSinceEpoch = split[1].toLongLong();

    if (timing.startTimeSecondsSinceEpoch != 0)
        timing.startTimeSecondsSinceEpoch -= kNtpUnixEpochDifferenceSeconds;

    if (timing.endTimeSecondsSinceEpoch != 0)
        timing.endTimeSecondsSinceEpoch -= kNtpUnixEpochDifferenceSeconds;

    return timing;
}

RepeatTimes Description::parseRepeatTimesLine(const QByteArray& line) const
{
    auto split = line.mid(kPrefixSize).split(L' ');

    if(split.size() < 3)
        return RepeatTimes();

    RepeatTimes repeatTimes;
    repeatTimes.repeatIntervalSeconds = parseTimeString(
        QString::fromUtf8(split[0]));
    repeatTimes.activeDurationSeconds = parseTimeString(
        QString::fromUtf8(split[1]));

    for (auto i = 2; i < split.size(); ++i)
    {
        repeatTimes.offsetsFromStartTimeSeconds.push_back(
            parseTimeString(QString::fromUtf8(split[i])));
    }

    return repeatTimes;
}

int64_t Description::parseTimeString(const QString& str) const
{
    QString timeStr = str;
    if (str.endsWith(kDayPostfix)
        || str.endsWith(kHourPostfix)
        || str.endsWith(kMinutePostfix)
        || str.endsWith(kSecondPostfix))
    {
        timeStr = str.left(str.size() - 1);
    }

    int multiplier = 1;

    if (str.endsWith(kDayPostfix))
        multiplier = kSecondsInDay;
    else if (str.endsWith(kHourPostfix))
        multiplier = kSecondsInHour;
    else if (str.endsWith(kMinutePostfix))
        multiplier = kSecondsInMinute;

    return timeStr.toLongLong() * multiplier;
}

MediaParams Description::parseMediaParamsLine(const QByteArray& line) const
{
    auto split = line.mid(kPrefixSize).split(L' ');

    if (split.size() < 4)
        return MediaParams();

    MediaParams mediaParams;
    mediaParams.mediaType = MediaParams::fromStringToMediaType(
        QString::fromUtf8(split[0]));
    
    auto portSplit = split[1].split(L'/');
    mediaParams.port = portSplit[0].toInt();

    if (portSplit.size() == 2)
        mediaParams.numberOfPorts = portSplit[1].toInt();

    mediaParams.proto = MediaParams::fromStringToProto(
        QString::fromUtf8(split[2]));
    mediaParams.format = QString::fromUtf8(split[3]);

    if (split.size() > 4)
    {
        for (auto i = 3; i < split.size(); ++i)
        {
            if (MediaParams::isRtpBasedProto(mediaParams.proto))
                mediaParams.rtpPayloadTypes.push_back(split[i].toInt());

            if (i == 3)
                continue;

            mediaParams.format += lit(" ") + QString::fromUtf8(split[i]);
        }
    }

    return mediaParams;
}

bool Description::assignSimpleStringByPrefix(const QString& prefix, const QString& value)
{
    if (prefix == kVersionPrefix)
        m_version = value;
    else if (prefix == kOriginPrefix)
        m_origin = value;
    else if (prefix == kSessionNamePrefix)
        m_sessionName = value;
    else if (prefix == kSessionInformationPrefix)
        m_sessionInformation = value;
    else if (prefix == kEmailPrefix)
        m_email = value;
    else if (prefix == kPhonePrefix)
        m_phone = value;
    else
        return false;

    return true;
}


template<typename Itr>
bool Description::parseMediaDescriptionLines(Itr& current, Itr& end)
{
    MediaDescription mediaDescription;
    mediaDescription.setMediaParams(parseMediaParamsLine(*current));
    ++current;

    while (current != end)
    {
        if (current->startsWith(kMediaTitlePrefix.toUtf8()))
        {
            auto prefixAndValue = parseSimpleStringLine(*current);
            mediaDescription.setMediaTitle(prefixAndValue.second);
        }
        else if (current->startsWith(kBandwidthPrefix.toUtf8()))
        {
            mediaDescription.setBandwidth(parseBandwidthLine(*current));
        }
        else if (current->startsWith(kConnectionInformationPrefix.toUtf8()))
        {
            mediaDescription.setConnectionInfo(parseConnectionInfoLine(*current));
        }
        else if (current->startsWith(kAttributePrefix.toUtf8()))
        {
            auto nameAndValue = parseAttributeLine(*current);
            mediaDescription.setAttribute(nameAndValue.first, nameAndValue.second);
        }
        else if (current->startsWith(kMediaDescriptionPrefix.toUtf8()))
        {
            break;
        }

        ++current;
    }

    m_mediaDescriptions.push_back(mediaDescription);

    return true;
}

template<typename Itr>
bool Description::parseTimeDescriptionLines(Itr& current, Itr& end)
{
    TimeDescription timeDescription;
    timeDescription.setTiming(parseTimingLine(*current));

    while (current != end)
    {
        if (current->startsWith(kRepeatTimesPrefix.toUtf8()))
            timeDescription.addRepeatTimes(parseRepeatTimesLine(*current));
        else
            break;
        
        ++current;
    }

    m_timeDescriptions.push_back(timeDescription);

    return true;
}

template<typename Itr>
bool Description::parseSessionLines(Itr& current, Itr& end)
{
    while (current != end)
    {
        if (current->startsWith(kTimingPrefix.toUtf8()))
        {
            parseTimeDescriptionLines(current, end);
            continue;
        }

        if (current->startsWith(kMediaDescriptionPrefix.toUtf8()))
        {
            parseMediaDescriptionLines(current, end);
            continue;
        }

        if (isSimpleStringLine(*current))
        {
            auto prefixAndValue = parseSimpleStringLine(*current);
            assignSimpleStringByPrefix(prefixAndValue.first, prefixAndValue.second);
        }
        else if (current->startsWith(kAttributePrefix.toUtf8()))
        {
            auto nameAndValue = parseAttributeLine(*current);
            m_attributes[nameAndValue.first] = nameAndValue.second;
        }
        else if (current->startsWith(kTimeZonePrefix.toUtf8()))
        {
            m_timeZones = parseTimeZoneLine(*current);
        }
        else if (current->startsWith(kBandwidthPrefix.toUtf8()))
        {
            m_bandwidth = parseBandwidthLine(*current);
        }
        else if (current->startsWith(kConnectionInformationPrefix.toUtf8()))
        {
            m_connectionInfo = parseConnectionInfoLine(*current);
        }
        else if (current->startsWith(kUriPrefix.toUtf8()))
        {
            m_url = parseUriLine(*current);
        }

        ++current;
    }

    return true;
}

} // namespace sdp
} // namespace network
} // namespace nx