#ifndef QN_CAMERA_ADVANCED_PARAM
#define QN_CAMERA_ADVANCED_PARAM

#include <functional>
#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>

#include <nx/fusion/model_functions_fwd.h>

struct QnCameraAdvancedParamValue
{
	QString id;
	QString value;

    QnCameraAdvancedParamValue() = default;
	QnCameraAdvancedParamValue(const QString &id, const QString &value);
};
#define QnCameraAdvancedParamValue_Fields (id)(value)

class QnCameraAdvancedParamValueMap: public QMap<QString, QString>
{

public:
    QnCameraAdvancedParamValueList toValueList() const;
    void appendValueList(const QnCameraAdvancedParamValueList &list);

    /** Get all values from this map that differs from corresponding values from other map. */
    QnCameraAdvancedParamValueList difference(const QnCameraAdvancedParamValueMap &other) const;

    bool differsFrom(const QnCameraAdvancedParamValueMap &other) const;
};

struct QnCameraAdvancedParamQueryInfo
{
    QString group;
    QString cmd;
};

struct QnCameraAdvancedParameterCondition
{
    enum class ConditionType
    {
        Equal,
        InRange,
        NotInRange,
        Default,
        Unknown
    };

    ConditionType type = ConditionType::Unknown;
    QString paramId;
    QString value;

    bool checkValue(const QString& valueToCheck) const;
    static ConditionType fromStringToConditionType(const QString& conditionTypeString);
    static QString fromConditionTypeToString(const ConditionType& conditionType);
};

QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParameterCondition::ConditionType, (lexical))

#define QnCameraAdvancedParameterCondition_Fields (type)(paramId)(value)

struct QnCameraAdvancedParameterDependency
{
    enum class DependencyType
    {
        Show,
        Range,
        Unknown
    };

    QString id;
    DependencyType type = DependencyType::Unknown;
    QString range;
    QString internalRange;
    std::vector<QnCameraAdvancedParameterCondition> conditions;

    static DependencyType fromStringToDependencyType(const QString& dependencyType);
    static QString fromDependencyTypeToString(const DependencyType& dependencyType);
};

QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParameterDependency::DependencyType, (lexical))

#define QnCameraAdvancedParameterDependency_Fields (id)(type)(range)(internalRange)(conditions)

struct QnCameraAdvancedParameter
{
    enum class DataType
    {
        None,
        Bool,
        Number,
        Enumeration,
        Button,
        String,
        Separator
    };

    QString id;
    DataType dataType = DataType::None;
    QString range;
    QString name;
    QString description;
    QString tag;  
    bool readOnly = false;
    QString readCmd; //< Read parameter command line. Isn't used in UI.
    QString writeCmd; //< Write parameter command line. Isn't used in UI.
    QString internalRange; //< Internal device values for range parameters.
    QString aux; //< Auxiliary driver dependent data.
    std::vector<QnCameraAdvancedParameterDependency> dependencies;
    bool showRange = false; //< Show range near parameter's label
    QString unit;
    QString notes;
    bool resync = false;

    bool isValid() const;
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

QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParameter::DataType, (lexical))
#define QnCameraAdvancedParameter_Fields (id)\
    (dataType)\
    (range)\
    (name)\
    (description)\
    (tag)\
    (readOnly)\
    (readCmd)\
    (writeCmd)\
    (internalRange)\
    (aux)\
    (dependencies)\
    (showRange)\
    (unit)\
    (notes)

struct QnCameraAdvancedParamGroup
{
    QString name;
    QString description;
    QString aux;
    std::vector<QnCameraAdvancedParamGroup> groups;
    std::vector<QnCameraAdvancedParameter> params;

    bool isEmpty() const;
    QSet<QString> allParameterIds() const;
    QnCameraAdvancedParameter getParameterById(const QString &id) const;
    bool updateParameter(const QnCameraAdvancedParameter &parameter);
    QnCameraAdvancedParamGroup filtered(const QSet<QString> &allowedIds) const;
};
#define QnCameraAdvancedParamGroup_Fields (name)(description)(aux)(groups)(params)

struct QnCameraAdvancedParameterOverload
{
    QString paramId;
    QString dependencyId;
    QString range;
    QString internalRange;
};
#define QnCameraAdvancedParameterOverload_Fields (paramId)(dependencyId)(range)(internalRange)

struct QnCameraAdvancedParams
{
    QString name;
    QString version;
    QString unique_id;
    bool packet_mode = false;
    std::vector<QnCameraAdvancedParamGroup> groups;

    QSet<QString> allParameterIds() const;
    QnCameraAdvancedParameter getParameterById(const QString &id) const;
    bool updateParameter(const QnCameraAdvancedParameter &parameter);

    void clear();
    QnCameraAdvancedParams filtered(const QSet<QString> &allowedIds) const;
    void applyOverloads(const std::vector<QnCameraAdvancedParameterOverload>& overloads);
};
#define QnCameraAdvancedParams_Fields (name)(version)(unique_id)(packet_mode)(groups)

#define QnCameraAdvancedParameterTypes (QnCameraAdvancedParamValue)\
    (QnCameraAdvancedParameter)\
    (QnCameraAdvancedParamGroup)\
    (QnCameraAdvancedParams)\
    (QnCameraAdvancedParameterDependency)\
    (QnCameraAdvancedParameterCondition)\
    (QnCameraAdvancedParameterOverload)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
	QnCameraAdvancedParameterTypes,
	(json)(metatype)
	)

Q_DECLARE_METATYPE(QnCameraAdvancedParamValueList)

#endif //QN_CAMERA_ADVANCED_PARAM
