#pragma once

#include <set>

#include <boost/optional/optional.hpp>

#include <QtCore/QXmlStreamReader>

#include <nx/network/buffer.h>

#include <plugins/resource/hanwha/hanwha_cgi_parameter.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaCgiParameters
{
public:
    explicit HanwhaCgiParameters() = default;
    HanwhaCgiParameters(const nx::Buffer& rawBuffer);
   
    boost::optional<HanwhaCgiParameter> parameter(
        const QString& submenu,
        const QString& action,
        const QString& parameter) const;

    boost::optional<HanwhaCgiParameter> parameter(const QString& path) const;

    bool isValid() const;

private:
    bool parseXml(const nx::Buffer& rawBuffer);

    bool parseSubmenus(QXmlStreamReader& reader);
    
    bool parseActions(QXmlStreamReader& reader, const QString& submenu);

    bool parseParameters(QXmlStreamReader& reader, const QString& submenu, const QString& action);

    bool parseDataType(
        QXmlStreamReader& reader,
        const QString& submenu,
        const QString& action,
        HanwhaCgiParameter& parameter);

private:
    using ParameterName = QString;
    using ActionName = QString;
    using SubmenuName = QString;

    using ParameterMap = std::map<ParameterName, HanwhaCgiParameter>;
    using ActionMap = std::map<ActionName, ParameterMap>;
    using SubmenuMap = std::map<SubmenuName, ActionMap>;

    bool m_isValid = false;
    SubmenuMap m_parameters;
};

} // namespace plugins
} // namespace mediaserve_core
} // namespace nx
