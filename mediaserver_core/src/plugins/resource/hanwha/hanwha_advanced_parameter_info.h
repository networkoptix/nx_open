#pragma once

#include <common/common_globals.h>
#include <core/resource/camera_advanced_param.h> 

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaAdavancedParameterInfo
{

public:
    HanwhaAdavancedParameterInfo(const QnCameraAdvancedParameter& parameter);

    QString supportAttribute() const;
    QString viewAction() const;
    QString updateAction() const;
    QString rangeParameter() const;

    Qn::ConnectionRole profileDependency() const;
    bool isChannelIndependent() const;
    bool isCodecDependent() const;
    QString resourceProperty() const;

    QString cgi() const;
    QString submenu() const;
    QString parameterName() const;
    QString parameterValue() const;

    bool isValid() const;

private:
    void parseParameter(const QnCameraAdvancedParameter& parameter);
    void parseAux(const QString& auxString);
    void parseId(const QString& idString);

private:
    QString m_supportAttribute;
    QString m_viewAction = lit("view");
    QString m_updateAction = lit("set");
    QString m_rangeParameter;

    Qn::ConnectionRole m_profile = Qn::ConnectionRole::CR_Default;
    bool m_channelIndependent = false;
    bool m_isCodecDependent = false;
    QString m_resourceProperty;

    QString m_cgi;
    QString m_submenu;
    QString m_parameterName;
    QString m_parameterValue;
};

} // namespace plugins
} // namespace meduiaserver_core
} // namespace nx
