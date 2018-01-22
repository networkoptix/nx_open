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
    QString toString() const;
};
#define QnCameraAdvancedParamValue_Fields (id)(value)

class QnCameraAdvancedParamValueMap: public QMap<QString, QString>
{
public:
    QnCameraAdvancedParamValueMap(const QnCameraAdvancedParamValueList& list = {});
    QnCameraAdvancedParamValueMap(std::initializer_list<std::pair<QString, QString>> values);

    QnCameraAdvancedParamValueList toValueList() const;
    void appendValueList(const QnCameraAdvancedParamValueList &list);
    QSet<QString> ids() const;

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
        equal, //< Watched value strictly equals to condition value
        inRange, //< Watched value is in condition value range
        notInRange,
        present, //< Watched parameter is present in parameter list
        notPresent,
        valueChanged,
        unknown
    };

    ConditionType type = ConditionType::unknown;
    QString paramId;
    QString value;

    bool checkValue(const QString& valueToCheck) const;
};

QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParameterCondition::ConditionType, (lexical))

#define QnCameraAdvancedParameterCondition_Fields (type)(paramId)(value)

struct QnCameraAdvancedParameterDependency
{
    enum class DependencyType
    {
        show,
        range,
        trigger,
        unknown
    };

    QString id;
    DependencyType type = DependencyType::unknown;
    QString range;
    QString internalRange;
    std::vector<QnCameraAdvancedParameterCondition> conditions;
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
    QString confirmation; //< Confirmation message. Actual only for the buttons now.
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
    bool shouldKeepInitialValue = false;
    bool bindDefaultToMinimum = false;

    bool isValid() const;
    bool isValueValid(const QString& value) const;
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
    (confirmation)\
    (tag)\
    (readOnly)\
    (readCmd)\
    (writeCmd)\
    (internalRange)\
    (aux)\
    (dependencies)\
    (showRange)\
    (unit)\
    (notes)\
    (shouldKeepInitialValue)\
    (bindDefaultToMinimum)\
    (resync)

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

    void merge(QnCameraAdvancedParams params);
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
