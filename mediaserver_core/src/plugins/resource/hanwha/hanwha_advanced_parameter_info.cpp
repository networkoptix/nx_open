#if defined(ENABLE_HANWHA)

#include "hanwha_advanced_parameter_info.h"
#include "hanwha_utils.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const QString kSupportAux = lit("support");
static const QString kRangeAux = lit("range");
static const QString kSpecificAux = lit("specific");
static const QString kProfileAux = lit("profile");
static const QString kNoChannelAux = lit("noChannel");
static const QString kCodecAux = lit("codec");
static const QString kResourceProperty = lit("resourceProperty");
static const QString kSortingAux = lit("sorting");
static const QString kGroupAux = lit("group");
static const QString kGroupLeadAux = lit("groupLead");
static const QString kGropupIncludeAux = lit("groupInclude");
static const QString kStreamsToReopenAux = lit("streamsToReopen");
static const QString kShouldAffectAllChannels = lit("shouldAffectAllChannels");

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
    :
    m_stringAuxes({
        {kSupportAux, &m_supportAttribute},
        {kRangeAux, &m_rangeParameter},
        {kResourceProperty, &m_resourceProperty},
        {kSortingAux, &m_sorting},
        {kGroupAux, &m_group},
        {kGropupIncludeAux, &m_groupIncludeCondition},
        {kGroupLeadAux, &m_group}
    }),
    m_boolAuxes({
        {kSpecificAux, &m_isSpecific},
        {kNoChannelAux, &m_channelIndependent},
        {kCodecAux, &m_isCodecDependent},
        {kShouldAffectAllChannels, &m_shouldAffectAllChannels}
    })
{
    parseParameter(parameter);
}

QString HanwhaAdavancedParameterInfo::id() const
{
    return m_id;
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

    return lit("%1/%2/%3/%4")
        .arg(m_cgi)
        .arg(m_submenu)
        .arg(m_updateAction)
        .arg(m_parameterName);
}

bool HanwhaAdavancedParameterInfo::isSpecific() const
{
    return m_isSpecific;
}

bool HanwhaAdavancedParameterInfo::isService() const
{
    return m_isService;
}

QSet<Qn::ConnectionRole> HanwhaAdavancedParameterInfo::streamsToReopen() const
{
    return m_streamsToReopen;
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

QString HanwhaAdavancedParameterInfo::sorting() const
{
    return m_sorting;
}

QString HanwhaAdavancedParameterInfo::group() const
{
    return m_group;
}

bool HanwhaAdavancedParameterInfo::isGroupLead() const
{
    return m_isGroupLead;
}

QString HanwhaAdavancedParameterInfo::groupIncludeCondition() const
{
    return m_groupIncludeCondition;
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
    return m_parameterValue;
}

bool HanwhaAdavancedParameterInfo::shouldAffectAllChannels() const
{
    return m_shouldAffectAllChannels;
}

bool HanwhaAdavancedParameterInfo::isValid() const
{
    return (!m_cgi.isEmpty()
        && !m_submenu.isEmpty()
        && !m_parameterName.isEmpty())
        || m_isService;
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
        const auto split = auxPart.trimmed().split(L'=');
        if (split.size() != 2)
            continue;

        const auto auxName = split[0].trimmed();
        const auto auxValue = split[1].trimmed();

        if (m_stringAuxes.find(auxName) != m_stringAuxes.cend())
            *m_stringAuxes.at(auxName) = auxValue;

        if (m_boolAuxes.find(auxName) != m_boolAuxes.cend())
            *m_boolAuxes.at(auxName) = fromString<bool>(auxValue);

        if (auxName == kProfileAux)
            m_profile = fromString<Qn::ConnectionRole>(auxValue);

        if (auxName == kGroupLeadAux)
            m_isGroupLead = true;

        if (auxName == kStreamsToReopenAux)
        {
            m_streamsToReopen.clear();
            const auto split = auxValue.split(L',');
            for (const auto& streamString: split)
            {
                const auto role = fromString<Qn::ConnectionRole>(streamString.trimmed());
                if (role != Qn::ConnectionRole::CR_Default)
                    m_streamsToReopen.insert(role);
            }
        }
    }
}

void HanwhaAdavancedParameterInfo::parseId(const QString& idString)
{
    m_id = idString;

    if (m_id.contains(lit("SERVICE%")))
    {
        m_isService = true;
        return;
    }

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

#endif // defined(ENABLE_HANWHA)

