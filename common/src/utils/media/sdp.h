#pragma once

#include <utility>
#include <vector>
#include <map>

#include <boost/optional/optional.hpp>

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QUrl>

#include <utils/media/sdp_common.h>

namespace nx {
namespace network {
namespace sdp {

class TimeDescription
{
public:
    Timing timing() const;
    std::vector<RepeatTimes> repeatTimes() const;

    void setTiming(const Timing& timing);

    void addRepeatTimes(const RepeatTimes& repeatTimes);
    void removeRepeatTimes(const RepeatTimes& repeatTimes);

    bool operator==(const TimeDescription& other);

private:
    Timing m_timing;
    std::vector<RepeatTimes> m_repeatTimes;
};

class MediaDescription
{
public:
    // Parsed from m= line
    MediaType mediaType() const;
    int port() const;
    int numberOfPorts() const;
    QString format() const;
    MediaParams mediaParams() const;
    
    // Parsed from other lines
    boost::optional<QString> mediaTitle() const;
    boost::optional<ConnectionInfo> connectionInfo() const;
    boost::optional<Bandwidth> bandwidth() const;
    boost::optional<QString> attribute(const QString& attributeName) const;
    std::map<QString, QString> attributes() const;

    void setMediaParams(const MediaParams& mediaParams);
    void setMediaType(MediaType mediaType);
    void setPort(int port);
    void setNumberOfPorts(int numberOfPorts);
    void setFormat(const QString& format);

    void setMediaTitle(const boost::optional<QString>& mediaTitle);
    void setConnectionInfo(const boost::optional<ConnectionInfo>& connectionInfo);
    void setBandwidth(const boost::optional<Bandwidth>& bandwidth);

    void setAttribute(const QString& attributeName, const QString& attributeValue);
    void removeAttribute(const QString& attributeName);

    bool operator==(const MediaDescription& other);
private:
    MediaParams m_mediaParams;

    boost::optional<QString> m_mediaTitle;
    boost::optional<ConnectionInfo> m_connectionInfo;
    boost::optional<Bandwidth> m_bandwidth;

    std::map<QString, QString> m_attributes;
};

class Description
{
public:
    bool decode(QByteArray& rawData);
    bool decode(QByteArrayList& rawData);

    QByteArray encode();

    QString version() const;
    QString origin() const;
    QString sessionName() const;
    boost::optional<QString> sessionInformation() const;
    boost::optional<QUrl> uri() const;
    boost::optional<QString> email() const;
    boost::optional<QString> phone() const;
    boost::optional<Bandwidth> bandwidth() const;
    boost::optional<ConnectionInfo> connectionInfo() const;
    
    std::vector<TimeDescription> timeDescriptions() const;
    std::vector<TimeZone> timeZones() const;

    std::map<QString, QString> attributes() const;
    boost::optional<QString> attribute(const QString& attributeName) const;
    std::vector<MediaDescription> mediaDescriptions() const;

    void setVersion(const QString& version);
    void setOrigin(const QString& origin);
    void setSessionName(const QString& sessionName);

    void setSessionInformation(const boost::optional<QString>& sessionInformation);
    void setUri(const boost::optional<QUrl>& url);
    void setEmail(const boost::optional<QString>& email);
    void setPhone(const boost::optional<QString>& phone);
    void setBandwidth(const boost::optional<Bandwidth>& bandwidth);
    void setConnectionInfo(const boost::optional<ConnectionInfo>& connectionInfo);

    void addTimeDescription(const TimeDescription& timeDescription);
    void removeTimeDescription(const TimeDescription& timeDescription);
    
    void addTimezone(const TimeZone& timeZone);
    void removeTimezone(const TimeZone& timeZone);

    void addAttribute(const QString& attributeName, const QString& attributeValue);
    void removeAttribute(const QString& attributeName);

    void addMediaDescription(const MediaDescription& mediaDescription);
    void removeMediaDescription(const MediaDescription& mediaDescription);

private:
    bool isSimpleStringLine(const QByteArray& line) const;
    bool assignSimpleStringByPrefix(const QString& prefix, const QString& value);

    std::pair<QString, QString> parseSimpleStringLine(const QByteArray& line) const;
    QUrl parseUriLine(const QByteArray& line) const;
    ConnectionInfo parseConnectionInfoLine(const QByteArray& line) const;
    Bandwidth parseBandwidthLine(const QByteArray& line) const;
    std::pair<QString, QString> parseAttributeLine(const QByteArray& line) const;
    std::vector<TimeZone> parseTimeZoneLine(const QByteArray& line) const;
    Timing parseTimingLine(const QByteArray& line) const;
    RepeatTimes parseRepeatTimesLine(const QByteArray& line) const;
    int64_t parseTimeString(const QString& str) const;
    MediaParams parseMediaParamsLine(const QByteArray& line) const;

    template<typename Itr>
    bool parseSessionLines(Itr& current, Itr& end);

    template<typename Itr>
    bool parseTimeDescriptionLines(Itr& current, Itr& end);

    template<typename Itr>
    bool parseMediaDescriptionLines(Itr& current, Itr& end);

private:
    QString m_version;
    QString m_origin;
    QString m_sessionName;
    boost::optional<QString> m_sessionInformation;
    boost::optional<QUrl> m_url;
    boost::optional<QString> m_email;
    boost::optional<QString> m_phone;
    boost::optional<Bandwidth> m_bandwidth;
    boost::optional<ConnectionInfo> m_connectionInfo;

    std::vector<TimeDescription> m_timeDescriptions;
    std::vector<TimeZone> m_timeZones;
    std::map<QString, QString> m_attributes;

    std::vector<MediaDescription> m_mediaDescriptions;
};

} // namespace sdp
} // namespace network
} // namespace nx