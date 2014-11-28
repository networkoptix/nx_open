#ifndef QN_CAMERA_ADVANCED_PARAM
#define QN_CAMERA_ADVANCED_PARAM

#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>

#include <utils/common/model_functions_fwd.h>
#include <utils/common/uuid.h>

class QDomElement;

struct QnCameraAdvancedParamValue {
	QString id;
	QString value;

	QnCameraAdvancedParamValue();
	QnCameraAdvancedParamValue(const QString &id, const QString &value);
};
#define QnCameraAdvancedParamValue_Fields (id)(value)

class QnCameraAdvancedParamValueMap: public QMap<QString, QString> {
public:
    QnCameraAdvancedParamValueMap();

    QnCameraAdvancedParamValueList toValueList() const;
    void appendValueList(const QnCameraAdvancedParamValueList &list);

    /** Get all values from this map that differs from corresponding values from other map. */
    QnCameraAdvancedParamValueList difference(const QnCameraAdvancedParamValueMap &other) const;
};

struct QnCameraAdvancedParameter {
    enum class DataType {
        None,
        Bool,
        Number,
        Enumeration,
        Button,
        String
    };


	bool isValid() const;

    QString id;
    DataType dataType;
    QString range;
    QString name;
    QString description;
    QString tag;  
    bool readOnly;

    QnCameraAdvancedParameter();

    QStringList getRange() const;
    void getRange(double &min, double &max) const;
    void setRange(int min, int max);
    void setRange(double min, double max);

	static QString dataTypeToString(DataType value);
	static DataType stringToDataType(const QString &value);

	static bool dataTypeHasValue(DataType value);
};
#define QnCameraAdvancedParameter_Fields (id)(dataType)(range)(name)(description)(tag)(readOnly)

struct QnCameraAdvancedParamGroup {
    QString name;
    QString description;
    std::vector<QnCameraAdvancedParamGroup> groups;
    std::vector<QnCameraAdvancedParameter> params;

    bool isEmpty() const;
    QSet<QString> allParameterIds() const;
    QnCameraAdvancedParameter getParameterById(const QString &id) const;
    bool updateParameter(const QnCameraAdvancedParameter &parameter);
    QnCameraAdvancedParamGroup filtered(const QSet<QString> &allowedIds) const;
};
#define QnCameraAdvancedParamGroup_Fields (name)(description)(groups)(params)

struct QnCameraAdvancedParams {
    QString name;
    QString version;
    QString unique_id;
    std::vector<QnCameraAdvancedParamGroup> groups;

    QSet<QString> allParameterIds() const;
    QnCameraAdvancedParameter getParameterById(const QString &id) const;
    bool updateParameter(const QnCameraAdvancedParameter &parameter);

    void clear();
    QnCameraAdvancedParams filtered(const QSet<QString> &allowedIds) const;
};
#define QnCameraAdvancedParams_Fields (name)(version)(unique_id)(groups)

/** Class for reading camera advanced parameters. */
class QnCameraAdvancedParamsReader {
public:
    QnCameraAdvancedParamsReader();

	/** Get parameters list for the given resource. */
	QnCameraAdvancedParams params(const QnResourcePtr &resource) const;
    void setParams(const QnResourcePtr &resource, const QnCameraAdvancedParams &params) const;

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

#define QnCameraAdvancedParameterTypes (QnCameraAdvancedParamValue)(QnCameraAdvancedParameter)(QnCameraAdvancedParamGroup)(QnCameraAdvancedParams)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
	QnCameraAdvancedParameterTypes,
	(json)(metatype)
	)

Q_DECLARE_METATYPE(QnCameraAdvancedParamValueList)

#endif //QN_CAMERA_ADVANCED_PARAM