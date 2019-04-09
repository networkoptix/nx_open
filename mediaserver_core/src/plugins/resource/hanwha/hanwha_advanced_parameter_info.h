#pragma once

#include <set>

#include <plugins/resource/hanwha/hanwha_common.h>

#include <common/common_globals.h>
#include <core/resource/camera_advanced_param.h>
#include <core/ptz/ptz_constants.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaAdavancedParameterInfo
{

public:
    HanwhaAdavancedParameterInfo(const QnCameraAdvancedParameter& parameter);

    QString id() const;
    QString supportAttribute() const;
    QString viewAction() const;
    QString updateAction() const;
    QString rangeParameter() const;
    bool isSpecific() const;
    bool isService() const;
    QSet<Qn::ConnectionRole> streamsToReopen() const;

    Qn::ConnectionRole profileDependency() const;
    bool isChannelIndependent() const;
    bool isCodecDependent() const;
    QString resourceProperty() const;
    QString sorting() const;
    QString group() const;
    QString groupIncludeCondition() const;
    bool isGroupLead() const;

    QString cgi() const;
    QString submenu() const;

    bool hasParameter() const;
    QString parameterName() const;
    QString parameterValue() const;
    bool shouldAffectAllChannels() const;

    // Parameter can be applied only to certain device types.
    bool isDeviceTypeSupported(HanwhaDeviceType deviceType) const;
    QSet<QString> associatedParameters() const;
    QString associationCondition() const;

    QSet<QString> ptzTraits() const;
    Ptz::Capabilities ptzCapabilities() const;

    bool isMulticastParameter() const;
    bool isValid() const;

private:
    void parseParameter(const QnCameraAdvancedParameter& parameter);
    void parseAux(const QString& auxString);
    void parseId(const QString& idString);

private:
    QString m_id;
    QString m_supportAttribute;
    QString m_viewAction = lit("view");
    QString m_updateAction = lit("set");
    QString m_rangeParameter;
    bool m_isSpecific = false;
    bool m_isService = false;
    QSet<Qn::ConnectionRole> m_streamsToReopen;

    Qn::ConnectionRole m_profile = Qn::ConnectionRole::CR_Default;
    bool m_channelIndependent = false;
    bool m_isCodecDependent = false;
    QString m_resourceProperty;
    QString m_sorting;
    QString m_group;
    QString m_groupIncludeCondition;
    bool m_isGroupLead = false;
    bool m_shouldAffectAllChannels = false;
    bool m_isMulticast = false;
    std::set<HanwhaDeviceType> m_deviceTypes;

    QString m_cgi;
    QString m_submenu;
    QString m_parameterName;
    QString m_parameterValue;

    QSet<QString> m_associatedParameters;
    QString m_associationCondition;
    QSet<QString> m_ptzTraits;
    Ptz::Capabilities m_ptzCapabilities = Ptz::NoPtzCapabilities;

    static const std::map<QString, QString HanwhaAdavancedParameterInfo::*> kStringAuxes;
    static const std::map<QString, bool HanwhaAdavancedParameterInfo::*> kBoolAuxes;
    static const std::map<QString, QSet<QString> HanwhaAdavancedParameterInfo::*> kStringSetAuxes;
};

} // namespace plugins
} // namespace meduiaserver_core
} // namespace nx
