#ifndef QN_CAMERA_ADVANCED_PARAM_READER_H
#define QN_CAMERA_ADVANCED_PARAM_READER_H

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_advanced_param.h>

class QDomElement;

/** Class for reading camera advanced parameters. */
class QnCameraAdvancedParamsReader {
public:
    QnCameraAdvancedParamsReader();

	/** Get parameters list for the given resource. */
	QnCameraAdvancedParams params(const QnResourcePtr &resource) const;
    void setParams(const QnResourcePtr &resource, const QnCameraAdvancedParams &params) const;

    static QString encodedParamsFromResource(const QnResourcePtr &resource);
    static void setEncodedParamsToResource(const QnResourcePtr &resource, const QString &params);

    static QnCameraAdvancedParams paramsFromResource(const QnResourcePtr &resource);
    static void setParamsToResource(const QnResourcePtr &resource, const QnCameraAdvancedParams &params);
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
};

#endif //QN_CAMERA_ADVANCED_PARAM_READER_H