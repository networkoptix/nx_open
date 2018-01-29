#pragma once

#include <set>

#include <boost/optional/optional.hpp>

#include <QtCore/QXmlStreamReader>

#include <nx/network/buffer.h>
#include <nx/network/http/http_types.h>

#include <plugins/resource/hanwha/hanwha_cgi_parameter.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaCgiParameters
{
public:
    HanwhaCgiParameters() = default;
    explicit HanwhaCgiParameters(nx::network::http::StatusCode::Value statusCode);
    explicit HanwhaCgiParameters(
        const nx::Buffer& rawBuffer,
        nx::network::http::StatusCode::Value statusCode);

    boost::optional<HanwhaCgiParameter> parameter(
        const QString& cgi,
        const QString& submenu,
        const QString& action,
        const QString& parameter) const;

    boost::optional<HanwhaCgiParameter> parameter(const QString& path) const;

    bool isValid() const;

    nx::network::http::StatusCode::Value statusCode() const;

private:
    bool parseXml(const nx::Buffer& rawBuffer);

    bool parseCgis(QXmlStreamReader& reader);

    bool parseSubmenus(QXmlStreamReader& reader, const QString& cgi);

    bool parseActions(QXmlStreamReader& reader, const QString& cgi, const QString& submenu);

    bool parseParameters(
        QXmlStreamReader& reader,
        const QString& cgi,
        const QString& submenu,
        const QString& action);

    bool parseDataType(
        QXmlStreamReader& reader,
        const QString& cgi,
        const QString& submenu,
        const QString& action,
        HanwhaCgiParameter& parameter);

private:
    using ParameterName = QString;
    using ActionName = QString;
    using SubmenuName = QString;
    using CgiName = QString;

    using ParameterMap = std::map<ParameterName, HanwhaCgiParameter>;
    using ActionMap = std::map<ActionName, ParameterMap>;
    using SubmenuMap = std::map<SubmenuName, ActionMap>;
    using CgiMap = std::map<CgiName, SubmenuMap>;

    bool m_isValid = false;
    CgiMap m_parameters;
    nx::network::http::StatusCode::Value m_statusCode;
};

} // namespace plugins
} // namespace mediaserve_core
} // namespace nx
