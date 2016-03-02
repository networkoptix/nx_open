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

    bool differsFrom(const QnCameraAdvancedParamValueMap &other) const;
};

struct QnCameraAdvancedAgregateParameterInfo
{
    QString agregateId;
    QString componentId;
    unsigned int componentPosition;
};

inline uint qHash(const QnCameraAdvancedAgregateParameterInfo& info, uint seed = 0)
{
    return qHash(info.agregateId, seed^0xa03f)^
            qHash(info.componentId, seed^0xa03f);
}

inline bool operator == (QnCameraAdvancedAgregateParameterInfo const& f, QnCameraAdvancedAgregateParameterInfo const& s)
{
    return f.agregateId == s.agregateId &&
            f.componentId == s.componentId &&
            f.componentPosition == s.componentPosition;
}

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
    QString readCmd; // read parameter command line. Isn't used in UI
    QString writeCmd; // write parameter command line. Isn't used in UI
    QString internalRange; // internal device values for range parameters

    QnCameraAdvancedParameter();

    QStringList getRange() const;
    QStringList getInternalRange() const;
    QString fromInternalRange(const QString& value) const;
    QString toInternalRange(const QString& value) const;

    void getRange(double &min, double &max) const;
    void setRange(int min, int max);
    void setRange(double min, double max);

	static QString dataTypeToString(DataType value);
	static DataType stringToDataType(const QString &value);

	static bool dataTypeHasValue(DataType value);
};
#define QnCameraAdvancedParameter_Fields (id)(dataType)(range)(name)(description)(tag)(readOnly)(readCmd)(writeCmd)(internalRange)

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
