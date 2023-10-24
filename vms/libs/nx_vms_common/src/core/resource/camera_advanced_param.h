// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <nx/fusion/model_functions_fwd.h>

struct NX_VMS_COMMON_API QnCameraAdvancedParamValue
{
    QString id;
    QString value;

    QnCameraAdvancedParamValue() = default;
    QnCameraAdvancedParamValue(const QString &id, const QString &value);
    QString toString() const;
};
#define QnCameraAdvancedParamValue_Fields (id)(value)

using QnCameraAdvancedParamValueList = QList<QnCameraAdvancedParamValue>;
using QnCameraAdvancedParamValueMap = QMap<QString, QString>;

class NX_VMS_COMMON_API QnCameraAdvancedParamValueMapHelper
{
public:
    static QnCameraAdvancedParamValueMap makeMap(const QnCameraAdvancedParamValueList& list = {});
    static QnCameraAdvancedParamValueMap makeMap(
        std::initializer_list<std::pair<QString, QString>> values);

    QnCameraAdvancedParamValueMapHelper(const QnCameraAdvancedParamValueMap& map);

    QnCameraAdvancedParamValueList toValueList() const;
    QnCameraAdvancedParamValueMap appendValueList(const QnCameraAdvancedParamValueList& list);

private:
    const QnCameraAdvancedParamValueMap& m_map;
};

struct NX_VMS_COMMON_API QnCameraAdvancedParameterCondition
{
    enum class ConditionType
    {
        /**%apidoc
         * Watched value strictly equals to condition value
         * %caption value
         */
        equal,

        /**%apidoc
         * %caption valueNe
         */
        notEqual,

        /**%apidoc
         * Watched value is in condition value range
         * %caption valueIn
         */
        inRange,

        /**%apidoc
         * %caption valueNotIn
         */
        notInRange,

        /**%apidoc
         * Watched parameter is present in parameter list
         * %caption present
         */
        present,

        /**%apidoc
         * %caption notPresent
         */
        notPresent,

        /**%apidoc
         * %caption valueChanged
         */
        valueChanged,

        /**%apidoc
         * %caption valueContains
         */
        contains,

        /**%apidoc[unused] */
        unknown
    };

    template<typename Visitor>
    friend constexpr auto nxReflectVisitAllEnumItems(ConditionType*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<ConditionType>;
        return visitor(
            Item{ConditionType::equal, "value"},
            Item{ConditionType::notEqual, "valueNe"},
            Item{ConditionType::inRange, "valueIn"},
            Item{ConditionType::notInRange, "valueNotIn"},
            Item{ConditionType::present, "present"},
            Item{ConditionType::notPresent, "notPresent"},
            Item{ConditionType::valueChanged, "valueChanged"},
            Item{ConditionType::contains, "valueContains"}
        );
    }

    QnCameraAdvancedParameterCondition() = default;

    QnCameraAdvancedParameterCondition(
        QnCameraAdvancedParameterCondition::ConditionType type,
        const QString& paramId,
        const QString& value);

    ConditionType type = ConditionType::unknown;
    QString paramId;
    QString value;

    bool operator==(const QnCameraAdvancedParameterCondition& other) const = default;

    bool checkValue(const QString& valueToCheck) const;
};

QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParameterCondition::ConditionType, (lexical))

#define QnCameraAdvancedParameterCondition_Fields (type)(paramId)(value)

struct NX_VMS_COMMON_API QnCameraAdvancedParameterDependency
{
    enum class DependencyType
    {
        /**%apidoc
         * %caption Show
         */
        show,

        /**%apidoc
         * %caption Range
         */
        range,

        /**%apidoc
         * %caption Trigger
         */
        trigger,

        /**%apidoc[unused] */
        unknown
    };

    template<typename Visitor>
    friend constexpr auto nxReflectVisitAllEnumItems(DependencyType*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<DependencyType>;
        return visitor(
            Item{DependencyType::show, "Show"},
            Item{DependencyType::range, "Range"},
            Item{DependencyType::trigger, "Trigger"}
        );
    }

    QString id;
    DependencyType type = DependencyType::unknown;
    QString range;
    QStringList valuesToAddToRange;
    QStringList valuesToRemoveFromRange;
    QString internalRange;
    std::vector<QnCameraAdvancedParameterCondition> conditions;

    bool operator==(const QnCameraAdvancedParameterDependency& other) const = default;

    /** Auto fill id field as a hash of dependency ids and values */
    void autoFillId(const QString& prefix = QString());
};

QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParameterDependency::DependencyType,
    (lexical),
    NX_VMS_COMMON_API)

#define QnCameraAdvancedParameterDependency_Fields (id)\
    (type)\
    (range)\
    (valuesToAddToRange)\
    (valuesToRemoveFromRange)\
    (internalRange)\
    (conditions)\

struct NX_VMS_COMMON_API QnCameraAdvancedParameter
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(DataType,
        None,
        Bool,
        Number,
        Enumeration,
        Button,
        String,
        Text,
        Separator,
        SliderControl, //< Vertical slider with additional buttons.
        PtrControl //< Pan tilt rotation control.
    )

    QString id;
    DataType dataType = DataType::None;
    QString range;
    QString name;
    QString description;
    QString confirmation; //< Confirmation message. Actual only for buttons now.
    QString actionName; //< Used as confirmation accept button text. "Yes" if empty.
    QString tag;
    bool availableInOffline = false;
    bool readOnly = false;
    QString readCmd; //< Read parameter command line. Isn't used in UI.
    QString writeCmd; //< Write parameter command line. Isn't used in UI.
    QString internalRange; //< Internal device values for range parameters.
    QString aux; //< Auxiliary driver dependent data.
    std::vector<QnCameraAdvancedParameterDependency> dependencies;
    bool showRange = false; //< Show range near parameter's label
    // If control can be packed with another such control in the same line.
    bool compact = false;
    QString unit;
    QString notes;
    // If a parameter with resync flag is changed then all parameters in a set should be reloaded.
    bool resync = false;
    bool keepInitialValue = false;
    bool bindDefaultToMinimum = false;
    // Parameters with the same group must be sent together
    // even if some of their values have not been changed.
    QString group;

    bool operator==(const QnCameraAdvancedParameter& other) const = default;

    bool isValid() const;
    bool isValueValid(const QString& value) const;
    QStringList getRange() const;
    QStringList getInternalRange() const;
    QString fromInternalRange(const QString& value) const;
    QString toInternalRange(const QString& value) const;

    void getRange(double &min, double &max) const;
    void setRange(int min, int max);
    void setRange(double min, double max);

    bool isCustomControl() const;

    /** Parameter datatype should have a value. */
    bool hasValue() const;

    /** Has not value, should be sent immediately. */
    bool isInstant() const;

    static bool dataTypeHasValue(DataType value);
};

QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParameter::DataType, (lexical), NX_VMS_COMMON_API)
#define QnCameraAdvancedParameter_Fields (id)\
    (dataType)\
    (range)\
    (name)\
    (description)\
    (confirmation)\
    (actionName)\
    (tag)\
    (readOnly)\
    (readCmd)\
    (writeCmd)\
    (internalRange)\
    (aux)\
    (dependencies)\
    (showRange)\
    (compact)\
    (unit)\
    (notes)\
    (keepInitialValue)\
    (bindDefaultToMinimum)\
    (resync)\
    (group)\
    (availableInOffline)

struct NX_VMS_COMMON_API QnCameraAdvancedParamGroup
{
    QString name;
    QString description;
    QString aux;
    std::vector<QnCameraAdvancedParamGroup> groups;
    std::vector<QnCameraAdvancedParameter> params;

    bool operator==(const QnCameraAdvancedParamGroup& other) const = default;

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

struct NX_VMS_COMMON_API QnCameraAdvancedParams
{
    QString name;
    QString version;
    QString pluginUniqueId;
    // If packet_mode is set to true then the value of all parameters should be reloaded
    // on every parameter change. May be considered as a group `QnCameraAdvancedParam::resync`
    bool packet_mode = false;
    std::vector<QnCameraAdvancedParamGroup> groups;

    bool operator==(const QnCameraAdvancedParams& other) const = default;

    QSet<QString> allParameterIds() const;
    QnCameraAdvancedParameter getParameterById(const QString &id) const;
    bool updateParameter(const QnCameraAdvancedParameter &parameter);

    void clear();
    QnCameraAdvancedParams filtered(const QSet<QString> &allowedIds) const;
    void applyOverloads(const std::vector<QnCameraAdvancedParameterOverload>& overloads);

    void merge(QnCameraAdvancedParams params);

    /** Check if at least one item is available when camera is offline. */
    bool hasItemsAvailableInOffline() const;
};
#define QnCameraAdvancedParams_Fields (name)(version)(pluginUniqueId)(packet_mode)(groups)

struct QnCameraAdvancedParamsMap: std::map<QnUuid, QnCameraAdvancedParams>
{
    using base_type = std::map<QnUuid, QnCameraAdvancedParams>;
    using base_type::base_type;
    const QnCameraAdvancedParams& front() const { return begin()->second; }
    QnUuid getId() const { return (size() == 1) ? begin()->first : QnUuid(); }
};

struct NX_VMS_COMMON_API QnCameraAdvancedParamsPostBody
{
    QString cameraId;
    QMap<QString, QString> paramValues;
};
#define QnCameraAdvancedParamsPostBody_Fields (cameraId)(paramValues)

QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParamValue, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParameter, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParamGroup, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParams, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParameterDependency,
    (json),
    NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParameterCondition,
    (json),
    NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParameterOverload, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnCameraAdvancedParamsPostBody, (json), NX_VMS_COMMON_API)
