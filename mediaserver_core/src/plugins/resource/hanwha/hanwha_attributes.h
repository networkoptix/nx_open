#pragma once

#if defined(ENABLE_HANWHA)

#include <QtCore/QXmlStreamReader>

#include <boost/optional/optional.hpp>

#include <nx/network/http/http_types.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaAttributes
{
    static const int kNoChannel = -1;
public:
    HanwhaAttributes() = default;
    explicit HanwhaAttributes(nx::network::http::StatusCode::Value statusCode);
    explicit HanwhaAttributes(const QString& attributesXml, nx::network::http::StatusCode::Value statusCode);

    template<typename T>
    boost::optional<T> attribute(
        const QString& group,
        const QString& attributeName,
        int channel = kNoChannel) const
    {
        static_assert(std::is_same<T, int>::value
            || std::is_same<T, double>::value
            || std::is_same<T, QString>::value
            || std::is_same<T, bool>::value,
            "Only QString, int, double and bool specializations are allowed");
    };

    template<typename T>
    boost::optional<T> attribute(const QString& path) const
    {
        auto split = path.split(L'/');
        auto splitSize = split.size();

        if (splitSize == 2)
            return attribute<T>(split[0], split[1], kNoChannel);

        if (splitSize == 3)
        {
            bool success = false;
            int channel  = split[2].toInt(&success);

            if (!success)
                return boost::none;

            return attribute<T>(split[0], split[1], channel);
        }

        return boost::none;
    }

    bool isValid() const;

    nx::network::http::StatusCode::Value statusCode() const;

private:

    boost::optional<QString> findAttribute(
        const QString& group,
        const QString& attributeName,
        int channel) const;

    void parseXml(QXmlStreamReader& reader, const QString& group, int channel);
    void parseAttribute(QXmlStreamReader& reader, const QString& group, int channel);

private:
    using GroupName = QString;
    using ChannelNumber = int;
    using AttributeName = QString;
    using AttributeValue = QString;
    using Groups = std::map<GroupName, std::map<AttributeName, AttributeValue>>;

    std::map<ChannelNumber, Groups> m_attributes;

private:
    bool m_isValid = false;
    nx::network::http::StatusCode::Value m_statusCode;
};

template<>
boost::optional<bool> HanwhaAttributes::attribute<bool>(
    const QString& group,
    const QString& attributeName,
    int channel) const;

template<>
boost::optional<QString> HanwhaAttributes::attribute<QString>(
    const QString& group,
    const QString& attributeName,
    int channel) const;

template<>
boost::optional<int> HanwhaAttributes::attribute<int>(
    const QString& group,
    const QString& attributeName,
    int channel) const;

template<>
boost::optional<double> HanwhaAttributes::attribute<double>(
    const QString& group,
    const QString& attributeName,
    int channel) const;

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
