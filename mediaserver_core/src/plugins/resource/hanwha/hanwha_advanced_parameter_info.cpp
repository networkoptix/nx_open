#include "hanwha_advanced_parameter_info.h"
#include "hanwha_utils.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const QString kSupportAux = lit("support");
static const QString kRangeAux = lit("range");
static const QString kProfileAux = lit("profile");
static const QString kNoChannelAux = lit("noChannel");
static const QString kCodecAux = lit("codec");
static const QString kResourceProperty = lit("resourceProperty");

static const QString kPrimaryProfile = lit("primary");
static const QString kSecondaryProfile = lit("secondary");

template<typename T>
T fromString(const QString&)
{
    return T();
}

template<>
bool fromString<bool>(const QString& str)
{
    return str == lit("1")
        || str.toLower() == lit("true");
}

template<>
Qn::ConnectionRole fromString<Qn::ConnectionRole>(const QString& str)
{
    if (str == kPrimaryProfile)
        return Qn::ConnectionRole::CR_LiveVideo;

    if (str == kSecondaryProfile)
        return Qn::ConnectionRole::CR_SecondaryLiveVideo;

    return Qn::ConnectionRole::CR_Default;
}

} // namespace nx

HanwhaAdavancedParameterInfo::HanwhaAdavancedParameterInfo(
    const QnCameraAdvancedParameter& parameter)
{
    parseParameter(parameter);
}

QString HanwhaAdavancedParameterInfo::supportAttribute() const
{
    return m_supportAttribute;
}

QString HanwhaAdavancedParameterInfo::viewAction() const
{
    return m_viewAction;
}

QString HanwhaAdavancedParameterInfo::updateAction() const
{
    return m_updateAction;
}

QString HanwhaAdavancedParameterInfo::rangeParameter() const
{
    if (!m_rangeParameter.isEmpty())
        return m_rangeParameter;

    qDebug() << "Range parameter: " << lit("%1/%2/%3/%4")
        .arg(m_cgi)
        .arg(m_submenu)
        .arg(m_updateAction)
        .arg(m_parameterName);

    return lit("%1/%2/%3/%4")
        .arg(m_cgi)
        .arg(m_submenu)
        .arg(m_updateAction)
        .arg(m_parameterName);
}

Qn::ConnectionRole HanwhaAdavancedParameterInfo::profileDependency() const
{
    return m_profile;
}

bool HanwhaAdavancedParameterInfo::isChannelIndependent() const
{
    return m_channelIndependent;
}

bool HanwhaAdavancedParameterInfo::isCodecDependent() const
{
    return m_isCodecDependent;
}

QString HanwhaAdavancedParameterInfo::resourceProperty() const
{
    return m_resourceProperty;
}

QString HanwhaAdavancedParameterInfo::cgi() const
{
    return m_cgi;
}

QString HanwhaAdavancedParameterInfo::submenu() const
{
    return m_submenu;
}

QString HanwhaAdavancedParameterInfo::parameterName() const
{
    return m_parameterName;
}

QString HanwhaAdavancedParameterInfo::parameterValue() const
{
    return m_parameterValue.isEmpty();
}

bool HanwhaAdavancedParameterInfo::isValid() const
{
    return !m_cgi.isEmpty()
        && !m_submenu.isEmpty()
        && !m_parameterName.isEmpty();
}

void HanwhaAdavancedParameterInfo::parseParameter(
    const QnCameraAdvancedParameter& parameter)
{
    parseId(parameter.id);

    if (!parameter.readCmd.isEmpty())
        m_viewAction = parameter.readCmd;

    if (!parameter.writeCmd.isEmpty())
        m_updateAction = parameter.writeCmd;

    if (!parameter.aux.isEmpty())
    {
        parseAux(parameter.aux);
    }
}

void HanwhaAdavancedParameterInfo::parseAux(const QString& auxString)
{
    const auto auxParts = auxString.split(L';');

    for (const auto& auxPart : auxParts)
    {
        const auto split = auxPart.split(L'=');
        if (split.size() != 2)
            continue;

        const auto auxName = split[0].trimmed();
        const auto auxValue = split[1].trimmed();

        if (auxName == kSupportAux)
            m_supportAttribute = auxValue;
        else if (auxName == kRangeAux)
            m_rangeParameter = auxValue;
        else if (auxName == kNoChannelAux)
            m_channelIndependent = fromString<bool>(auxValue);
        else if (auxName == kProfileAux)
            m_profile = fromString<Qn::ConnectionRole>(auxValue);
        else if (auxName == kCodecAux)
            m_isCodecDependent = fromString<bool>(auxValue);
        else if (auxName == kResourceProperty)
            m_resourceProperty = auxValue;
    }
}

void HanwhaAdavancedParameterInfo::parseId(const QString& idString)
{
    QString idInfoPart;
    auto split = idString.split(L'%');
    if (split.size() == 2)
        idInfoPart = split[1];
    else
        idInfoPart = split[0];

    split = idInfoPart.split(L'/');
    if (split.size() != 3)
        return;

    m_cgi = split[0];
    m_submenu = split[1];

    const auto nameAndValue = split[2].split(L'=');
    if (nameAndValue.isEmpty())
        return;

    m_parameterName = nameAndValue[0];
    if (nameAndValue.size() == 2)
        m_parameterValue = nameAndValue[1];
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
