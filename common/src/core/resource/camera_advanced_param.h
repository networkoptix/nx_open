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
};
#define QnCameraAdvancedParameter_Fields (name)(description)(query)(dataType)(method)(min)(max)(step)(readOnly)

struct QnCameraAdvancedParamGroup {
    QString name;
    QString description;
    std::vector<QnCameraAdvancedParamGroup> groups;
    std::vector<QnCameraAdvancedParameter> params;

	void merge(const QnCameraAdvancedParamGroup &other);
};
#define QnCameraAdvancedParamGroup_Fields (name)(description)(groups)(params)

struct QnCameraAdvancedParams {
    std::vector<QnCameraAdvancedParamGroup> groups;

	void merge(const QnCameraAdvancedParams &other);
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

	/** Parse xml and add its contents to the inner structure. */
	QnCameraAdvancedParamsTree readXml(QIODevice *xmlSource) const;

	QnCameraAdvancedParams parseCameraXml(const QDomElement &cameraXml) const;
	QnCameraAdvancedParamGroup parseGroupXml(const QDomElement &groupXml) const;
	QnCameraAdvancedParameter parseElementXml(const QDomElement &elementXml) const;

	void buildTree(const QMultiMap<QString, QnCameraAdvancedParamsTree> &source, QnCameraAdvancedParamsTree &out) const;

private:
	/* Per-camera parameters cache. */
	mutable QHash<QnUuid, QnCameraAdvancedParams> m_paramsByCameraId;

	/* Default parameters tree. */
	mutable QnCameraAdvancedParamsTree m_defaultParamsTree;
};

#define QnCameraAdvancedParameterTypes (QnCameraAdvancedParamValue)(QnCameraAdvancedParameter)(QnCameraAdvancedParamGroup)(QnCameraAdvancedParams)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
	QnCameraAdvancedParameterTypes,
	(json)(metatype)
	)

Q_DECLARE_METATYPE(QnCameraAdvancedParamValueList)

#endif //QN_CAMERA_ADVANCED_PARAM