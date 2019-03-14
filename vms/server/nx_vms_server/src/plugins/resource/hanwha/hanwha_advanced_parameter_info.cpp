#include "hanwha_advanced_parameter_info.h"
#include "hanwha_utils.h"

#include <nx/fusion/serialization/lexical.h>

namespace nx {
namespace vms::server {
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
static const QString kDeviceTypesAux = lit("deviceTypes");
static const QString kAssociatedParametersAux = lit("associatedWith");
static const QString kPtzTraitsAux = lit("ptzTraits");
static const QString kPtzCapabilitiesAux = lit("ptzCapabilities");

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
QSet<QString> fromString<QSet<QString>>(const QString& str)
{
    return str.split(L',', QString::SplitBehavior::SkipEmptyParts).toSet();
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

} // namespace

const std::map<QString, QString HanwhaAdavancedParameterInfo::*>
HanwhaAdavancedParameterInfo::kStringAuxes = {
    {kSupportAux, &HanwhaAdavancedParameterInfo::m_supportAttribute},
    {kRangeAux, &HanwhaAdavancedParameterInfo::m_rangeParameter},
    {kResourceProperty, &HanwhaAdavancedParameterInfo::m_resourceProperty},
    {kSortingAux, &HanwhaAdavancedParameterInfo::m_sorting},
    {kGroupAux, &HanwhaAdavancedParameterInfo::m_group},
    {kGropupIncludeAux, &HanwhaAdavancedParameterInfo::m_groupIncludeCondition},
    {kGroupLeadAux, &HanwhaAdavancedParameterInfo::m_group}
};

const std::map<QString, bool HanwhaAdavancedParameterInfo::*>
HanwhaAdavancedParameterInfo::kBoolAuxes = {
    {kSpecificAux, &HanwhaAdavancedParameterInfo::m_isSpecific},
    {kNoChannelAux, &HanwhaAdavancedParameterInfo::m_channelIndependent},
    {kCodecAux, &HanwhaAdavancedParameterInfo::m_isCodecDependent},
    {kShouldAffectAllChannels, &HanwhaAdavancedParameterInfo::m_shouldAffectAllChannels}
};

const std::map<QString, QSet<QString> HanwhaAdavancedParameterInfo::*>
HanwhaAdavancedParameterInfo::kStringSetAuxes = {
    {kAssociatedParametersAux, &HanwhaAdavancedParameterInfo::m_associatedParameters},
    {kPtzTraitsAux, &HanwhaAdavancedParameterInfo::m_ptzTraits},
};


HanwhaAdavancedParameterInfo::HanwhaAdavancedParameterInfo(
    const QnCameraAdvancedParameter& parameter)
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

bool HanwhaAdavancedParameterInfo::hasParameter() const
{
    return !m_parameterName.isEmpty();
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

bool HanwhaAdavancedParameterInfo::isDeviceTypeSupported(HanwhaDeviceType deviceType) const
{
    if (m_deviceTypes.empty())
        return true;

    return m_deviceTypes.find(deviceType) != m_deviceTypes.cend();
}

QSet<QString> HanwhaAdavancedParameterInfo::associatedParameters() const
{
    return m_associatedParameters;
}

QSet<QString> HanwhaAdavancedParameterInfo::ptzTraits() const
{
    return m_ptzTraits;
}

Ptz::Capabilities HanwhaAdavancedParameterInfo::ptzCapabilities() const
{
    return m_ptzCapabilities;
}

bool HanwhaAdavancedParameterInfo::isValid() const
{
    return (!m_cgi.isEmpty()
        && !m_submenu.isEmpty())
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

    for (const auto& auxPart: auxParts)
    {
        const auto split = auxPart.trimmed().split(L'=');
        if (split.size() != 2)
            continue;

        const auto auxName = split[0].trimmed();
        const auto auxValue = split[1].trimmed();

        if (kStringAuxes.find(auxName) != kStringAuxes.cend())
            this->*kStringAuxes.at(auxName) = auxValue;

        if (kBoolAuxes.find(auxName) != kBoolAuxes.cend())
            this->*kBoolAuxes.at(auxName) = fromString<bool>(auxValue);

        if (kStringSetAuxes.find(auxName) != kStringSetAuxes.cend())
            this->*kStringSetAuxes.at(auxName) = fromString<QSet<QString>>(auxValue);

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
        if (auxName == kDeviceTypesAux)
        {
            m_deviceTypes.clear();
            const auto split = auxValue.split(L',');
            for (const auto& deviceTypeString: split)
            {
                const auto deviceType = QnLexical::deserialized<HanwhaDeviceType>(
                    deviceTypeString.trimmed(),
                    HanwhaDeviceType::unknown);

                m_deviceTypes.insert(deviceType);
            }
        }

        if (auxName == kPtzCapabilitiesAux)
        {
            m_ptzCapabilities = QnLexical::deserialized<Ptz::Capabilities>(
                auxValue,
                Ptz::NoPtzCapabilities);
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
    if (split.size() != 2 && split.size() != 3)
    {
        NX_ASSERT(false, lm("Wrong parameter id: %1").args(idString));
        return;
    }

    m_cgi = split[0];
    m_submenu = split[1];

    if (split.size() == 3)
    {
        const auto nameAndValue = split[2].split(L'=');
        if (nameAndValue.isEmpty())
            return;

        m_parameterName = nameAndValue[0];
        if (nameAndValue.size() == 2)
            m_parameterValue = nameAndValue[1];
    }
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
