#ifndef QN_CAMERA_ADVANCED_PARAM
#define QN_CAMERA_ADVANCED_PARAM

#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>

#include <utils/common/model_functions_fwd.h>

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
    bool packet_mode;
    std::vector<QnCameraAdvancedParamGroup> groups;

    QnCameraAdvancedParams();

    QSet<QString> allParameterIds() const;
    QnCameraAdvancedParameter getParameterById(const QString &id) const;
    bool updateParameter(const QnCameraAdvancedParameter &parameter);

    void clear();
    QnCameraAdvancedParams filtered(const QSet<QString> &allowedIds) const;
};
#define QnCameraAdvancedParams_Fields (name)(version)(unique_id)(packet_mode)(groups)

#define QnCameraAdvancedParameterTypes (QnCameraAdvancedParamValue)(QnCameraAdvancedParameter)(QnCameraAdvancedParamGroup)(QnCameraAdvancedParams)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
	QnCameraAdvancedParameterTypes,
	(json)(metatype)
	)

Q_DECLARE_METATYPE(QnCameraAdvancedParamValueList)

#endif //QN_CAMERA_ADVANCED_PARAM