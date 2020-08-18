#ifndef QN_CAMERA_ADVANCED_PARAM_READER_H
#define QN_CAMERA_ADVANCED_PARAM_READER_H

#include <core/resource/camera_advanced_param.h>

class QDomElement;

class QnCameraAdvacedParamsXmlParser {
public:
    static bool validateXml(QIODevice *xmlSource);
    static bool readXml(QIODevice *xmlSource, QnCameraAdvancedParams &result);
private:
    static bool parsePluginXml(const QDomElement &pluginXml, QnCameraAdvancedParams &params);
    static bool parseGroupXml(const QDomElement &groupXml, QnCameraAdvancedParamGroup &group);
    static bool parseElementXml(const QDomElement &elementXml, QnCameraAdvancedParameter &param);

    static bool parseDependencyGroupsXml(
        const QDomElement& elementXml,
        std::vector<QnCameraAdvancedParameterDependency>& dependencies);

    static bool parseDependenciesXml(
        const QDomElement& elementXml,
        std::vector<QnCameraAdvancedParameterDependency>& dependencies);

    static bool parseConditionsXml(
        const QDomElement& elementXml,
        std::vector<QnCameraAdvancedParameterCondition>& conditions);

    static bool parseConditionString(
        QString conditionString,
        QnCameraAdvancedParameterCondition& condition);
};

#endif //QN_CAMERA_ADVANCED_PARAM_READER_H
