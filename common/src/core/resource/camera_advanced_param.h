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
        MinMaxStep,
        Enumeration,
        Button,
        String,
        ControlButtonsPair
    };

	QString getId() const;
	bool isValid() const;

    QString name;
    QString description;
    QString query;
    DataType dataType;
    QString method;
    QString min;
    QString max;
    QString step;
    bool readOnly;

    QnCameraAdvancedParameter();

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

	void merge(const QnCameraAdvancedParamGroup &other);

    QnCameraAdvancedParameter getParameterById(const QString &id) const;
};
#define QnCameraAdvancedParamGroup_Fields (name)(description)(groups)(params)

struct QnCameraAdvancedParams {
    std::vector<QnCameraAdvancedParamGroup> groups;

	void merge(const QnCameraAdvancedParams &other);

    QnCameraAdvancedParameter getParameterById(const QString &id) const;
};
#define QnCameraAdvancedParams_Fields (groups)

struct QnCameraAdvancedParamsTree {
	QString cameraTypeName;
	std::vector<QnCameraAdvancedParamsTree> children;

	QnCameraAdvancedParams params;

	bool isEmpty() const;

	bool containsSubTree(const QString &cameraTypeName) const;
	QnCameraAdvancedParams flatten(const QString &cameraTypeName = QString()) const;
};

/** Class for reading camera advanced parameters xml file. */
class QnCameraAdvancedParamsReader {
public:
    QnCameraAdvancedParamsReader();

	/** Get parameters tree for the given camera. */
	QnCameraAdvancedParams params(const QnResourcePtr &resource) const;
private:
	/** Get inner camera type that is used in the xml as node id. */
	QString calculateCameraType(const QnResourcePtr &resource) const;

private:
	/* Per-camera parameters cache. */
	mutable QHash<QnUuid, QnCameraAdvancedParams> m_paramsByCameraId;

	/* Default parameters tree. */
	mutable QnCameraAdvancedParamsTree m_defaultParamsTree;
};

class QnCameraAdvacedParamsXmlParser {
public:
	static QnCameraAdvancedParamsTree readXml(QIODevice *xmlSource);
private:
	static QnCameraAdvancedParams parseCameraXml(const QDomElement &cameraXml);
	static QnCameraAdvancedParamGroup parseGroupXml(const QDomElement &groupXml);
	static QnCameraAdvancedParameter parseElementXml(const QDomElement &elementXml);

	static void buildTree(const QMultiMap<QString, QnCameraAdvancedParamsTree> &source, QnCameraAdvancedParamsTree &out);
};

#define QnCameraAdvancedParameterTypes (QnCameraAdvancedParamValue)(QnCameraAdvancedParameter)(QnCameraAdvancedParamGroup)(QnCameraAdvancedParams)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
	QnCameraAdvancedParameterTypes,
	(json)(metatype)
	)

Q_DECLARE_METATYPE(QnCameraAdvancedParamValueList)

#endif //QN_CAMERA_ADVANCED_PARAM