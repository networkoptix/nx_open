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

    QString name;
    QString description;
    QString id;
    DataType dataType;
    QString tag;
    QString range;
    bool readOnly;

    QnCameraAdvancedParameter();

    QStringList getRange() const;
    void setRange(int min, int max);
    void setRange(double min, double max);

	static QString dataTypeToString(DataType value);
	static DataType stringToDataType(const QString &value);

	static bool dataTypeHasValue(DataType value);
};
#define QnCameraAdvancedParameter_Fields (name)(description)(query)(dataType)(method)(min)(max)(step)(readOnly)

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

/** Class for reading camera advanced parameters xml file. */
class QnCameraAdvancedParamsReader {
public:
    QnCameraAdvancedParamsReader();

	/** Get parameters list for the given resource. */
	QnCameraAdvancedParams params(const QnResourcePtr &resource) const;

    /** Get set of allowed params id's for the given resource. */
    static QSet<QString> allowedParams(const QnResourcePtr &resource);
private:
	/** Get inner camera type that is used in the xml as node id. */
	QString calculateCameraType(const QnResourcePtr &resource) const;
private:
	/* Per-camera parameters cache. */
	mutable QHash<QString, QnCameraAdvancedParams> m_paramsByCameraType;
};

class QnCameraAdvacedParamsXmlParser {
public:
    static bool validateXml(QIODevice *xmlSource);
	static bool readXml(QIODevice *xmlSource, QnCameraAdvancedParams &result);
private:
	static bool parseCameraXml(const QDomElement &cameraXml, QnCameraAdvancedParams &params);
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