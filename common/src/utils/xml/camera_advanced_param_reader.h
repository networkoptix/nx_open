#ifndef QN_CAMERA_ADVANCED_PARAM_READER_H
#define QN_CAMERA_ADVANCED_PARAM_READER_H

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_advanced_param.h>

#include <utils/common/connective.h>

#include <nx/utils/uuid.h>

class QDomElement;

/** Static class for reading camera advanced parameters. */
class QnCameraAdvancedParamsReader {
public:
    static QString encodedParamsFromResource(const QnResourcePtr &resource);
    static void setEncodedParamsToResource(const QnResourcePtr &resource, const QString &params);

    static QnCameraAdvancedParams paramsFromResource(const QnResourcePtr &resource);
    static void setParamsToResource(const QnResourcePtr &resource, const QnCameraAdvancedParams &params);
};

class QnCachingCameraAdvancedParamsReader: public Connective<QObject>, protected QnCameraAdvancedParamsReader {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnCachingCameraAdvancedParamsReader(QObject* parent = nullptr);

    /** Get parameters list for the given resource. */
    QnCameraAdvancedParams params(const QnResourcePtr &resource) const;
    void setParams(const QnResourcePtr &resource, const QnCameraAdvancedParams &params) const;
    void clearResourceParams(const QnResourcePtr& resource);
private:
    /* Per-camera parameters cache. */
    mutable QHash<QnUuid, QnCameraAdvancedParams> m_paramsByCameraId;
};

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
