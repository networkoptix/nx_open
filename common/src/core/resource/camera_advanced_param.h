#ifndef QN_CAMERA_ADVANCED_PARAM
#define QN_CAMERA_ADVANCED_PARAM

#include <core/resource/resource_fwd.h>

#include <utils/common/model_functions_fwd.h>

struct CameraAdvancedParameter {
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

    QString name;
    QString description;
    QString query;
    DataType dataType;
    QString method;
    QString min;
    QString max;
    QString step;
    bool readOnly;

    CameraAdvancedParameter();

	static QString dataTypeToString(DataType value);
	static DataType stringToDataType(const QString &value);
};
#define CameraAdvancedParameter_Fields (name)(description)(query)(dataType)(method)(min)(max)(step)(readOnly)

struct CameraAdvancedParamGroup {
    QString name;
    QString description;
    std::vector<CameraAdvancedParamGroup> groups;
    std::vector<CameraAdvancedParameter> params;

	void merge(const CameraAdvancedParamGroup &other);
};
#define CameraAdvancedParamGroup_Fields (name)(description)(groups)(params)

struct CameraAdvancedParams {
    std::vector<CameraAdvancedParamGroup> groups;

	void merge(const CameraAdvancedParams &other);
};
#define CameraAdvancedParams_Fields (groups)

struct CameraAdvancedParamsTree {
	QString cameraTypeName;
	std::vector<CameraAdvancedParamsTree> children;

	CameraAdvancedParams params;

	bool isEmpty() const;

	bool containsSubTree(const QString &cameraTypeName) const;
	CameraAdvancedParams flatten(const QString &cameraTypeName = QString()) const;
};

/** Class for reading camera advanced parameters xml file. */
class QnCameraAdvancedParamsReader {
public:
    QnCameraAdvancedParamsReader();

	/** Get parameters tree for the given camera. */
	CameraAdvancedParams params(const QnResourcePtr &resource) const;
private:
	/** Get inner camera type that is used in the xml as node id. */
	QString calculateCameraType(const QnResourcePtr &resource) const;

	/** Parse xml and add its contents to the inner structure. */
	CameraAdvancedParamsTree readXml(QIODevice *xmlSource) const;

	CameraAdvancedParams parseCameraXml(const QDomElement &cameraXml) const;
	CameraAdvancedParamGroup parseGroupXml(const QDomElement &groupXml) const;
	CameraAdvancedParameter parseElementXml(const QDomElement &elementXml) const;

	void buildTree(const QMultiMap<QString, CameraAdvancedParamsTree> &source, CameraAdvancedParamsTree &out) const;

private:
	/* Per-camera parameters cache. */
	mutable QHash<QnUuid, CameraAdvancedParams> m_paramsByCameraId;

	/* Default parameters tree. */
	mutable CameraAdvancedParamsTree m_defaultParamsTree;
};

#define CameraAdvancedParameterTypes (CameraAdvancedParameter)(CameraAdvancedParamGroup)(CameraAdvancedParams)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
	CameraAdvancedParameterTypes,
	(json)
	)

#endif //QN_CAMERA_ADVANCED_PARAM