#include "camera_advanced_param.h"

#include <boost/range.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/range/algorithm/find_if.hpp>

#include <core/resource/resource.h>
#include <core/resource/param.h>

#include <nx/utils/log/log.h>
#include <nx/fusion/model_functions.h>

using ConditionType = QnCameraAdvancedParameterCondition::ConditionType;
using DependencyType = QnCameraAdvancedParameterDependency::DependencyType;

namespace {

const QString kEqualConditionType = lit("value");
const QString kInRangeConditionType = lit("valueIn");
const QString kNotInRangeConditionType = lit("valueNotIn");
const QString kPresenceConditionType = lit("present");
const QString kLackOfPresenceConditionType = lit("notPresent");
const QString kValueChangedConditionType = lit("valueChanged");

const QString kShowDependencyType = lit("Show");
const QString kRangeDependencyType = lit("Range");
const QString kValueDependencyType = lit("DefaultValue");

const QString kBoolDataType = lit("Bool");
const QString kNumberDataType = lit("Number");
const QString kEnumerationDataType = lit("Enumeration");
const QString kButtonDataType = lit("Button");
const QString kStringDataType = lit("String");
const QString kSeparatorDataType = lit("Separator");

} //< anonymous namespace

QnCameraAdvancedParamValue::QnCameraAdvancedParamValue(const QString &id, const QString &value):
	id(id), value(value)
{
}

QString QnCameraAdvancedParamValue::toString() const
{
    return lm("%1 = %2").args(id, value);
}

QnCameraAdvancedParamValueMap::QnCameraAdvancedParamValueMap(
    const QnCameraAdvancedParamValueList& list)
{
    appendValueList(list);
}


QnCameraAdvancedParamValueMap::QnCameraAdvancedParamValueMap(
    std::initializer_list<std::pair<QString, QString>> values)
{
    for (const auto& v: values)
        insert(v.first, v.second);
}

QnCameraAdvancedParamValueList QnCameraAdvancedParamValueMap::toValueList() const
{
    QnCameraAdvancedParamValueList result;
    for (auto iter = this->cbegin(); iter != this->cend(); ++iter)
        result << QnCameraAdvancedParamValue(iter.key(), iter.value());
    return result;
}

QnCameraAdvancedParamValueList QnCameraAdvancedParamValueMap::difference(const QnCameraAdvancedParamValueMap &other) const
{
    QnCameraAdvancedParamValueList result;
    for (auto iter = this->cbegin(); iter != this->cend(); ++iter) {
        if (other.contains(iter.key()) && other[iter.key()] == iter.value())
            continue;
        result << QnCameraAdvancedParamValue(iter.key(), iter.value());
    }
    return result;
}

bool QnCameraAdvancedParamValueMap::differsFrom(const QnCameraAdvancedParamValueMap &other) const
{
    for (auto it = this->cbegin(); it != this->cend(); ++it)
    {
        if (!other.contains(it.key()) || other.value(it.key()) != it.value())
            return true;
    }
    return false;
}

void QnCameraAdvancedParamValueMap::appendValueList(const QnCameraAdvancedParamValueList &list)
{
    for (const QnCameraAdvancedParamValue &value: list)
        insert(value.id, value.value);
}

QSet<QString> QnCameraAdvancedParamValueMap::ids() const
{
    QSet<QString> set;
    for (const auto& id: this->keys())
        set.insert(id);

    return set;
}

bool QnCameraAdvancedParameter::isValid() const
{
	return (dataType != DataType::None)
        && (!id.isEmpty());
}

bool QnCameraAdvancedParameter::isValueValid(const QString& value) const
{
    if (dataType == DataType::None)
        return false;

    if (dataType == DataType::Bool)
        return value == lit("true") || value == lit("false");

    if (dataType == DataType::Number)
    {
        bool isOk = false;
        const auto number = value.toDouble(&isOk);
        if (!isOk)
            return false;

        if (range.isEmpty())
            return true;

        double min = 0, max = 0;
        getRange(min, max);
        return number >= min && number <= max;
    }

    if (dataType == DataType::Enumeration)
        return range.isEmpty() || getRange().contains(value);

    // TODO: Add some more checks?
    return true;
}

QString QnCameraAdvancedParameter::dataTypeToString(DataType value)
{
    switch (value)
    {
        case DataType::Bool:
            return kBoolDataType;
        case DataType::Number:
            return kNumberDataType;
        case DataType::Enumeration:
            return kEnumerationDataType;
        case DataType::Button:
            return kButtonDataType;
        case DataType::String:
            return kStringDataType;
        case DataType::Separator:
            return kSeparatorDataType;
        default:
            return QString();
    }
}

QnCameraAdvancedParameter::DataType QnCameraAdvancedParameter::stringToDataType(const QString &value)
{
	QList<QnCameraAdvancedParameter::DataType> allDataTypes = QList<QnCameraAdvancedParameter::DataType>()
		<< DataType::Bool
		<< DataType::Number
		<< DataType::Enumeration
		<< DataType::Button
		<< DataType::String
        << DataType::Separator;

	for (auto dataType: allDataTypes)
		if (dataTypeToString(dataType) == value)
			return dataType;
	return DataType::None;
}

bool QnCameraAdvancedParameter::dataTypeHasValue(DataType value)
{
	switch (value) {
	case DataType::Bool:
	case DataType::Number:
	case DataType::Enumeration:
	case DataType::String:
		return true;
	case DataType::Button:
    case DataType::Separator:
        return false;
    default:
        return false;
    }
}

QStringList QnCameraAdvancedParameter::getRange() const
{
    NX_ASSERT(dataType == DataType::Enumeration);
    return range.split(L',', QString::SkipEmptyParts);
}

QStringList QnCameraAdvancedParameter::getInternalRange() const
{
    NX_ASSERT(dataType == DataType::Enumeration);
    return internalRange.split(L',', QString::SkipEmptyParts);
}

QString QnCameraAdvancedParameter::fromInternalRange(const QString& value) const
{
    const auto outer = getRange();
    const auto inner = getInternalRange();
    for (int i = 0; i < qMin(outer.size(), inner.size()); ++i) {
        if (inner[i] == value)
            return outer[i];
    }
    return value;
}

QString QnCameraAdvancedParameter::toInternalRange(const QString& value) const
{
    const auto outer = getRange();
    const auto inner = getInternalRange();
    for (int i = 0; i < qMin(outer.size(), inner.size()); ++i) {
        if (outer[i] == value)
            return inner[i];
    }
    return value;
}

void QnCameraAdvancedParameter::getRange(double &min, double &max) const
{
    NX_ASSERT(dataType == DataType::Number);
    QStringList values = range.split(L',');
    NX_ASSERT(values.size() == 2);
    if (values.size() != 2)
        return;
    min = values[0].toDouble();
    max = values[1].toDouble();
}

void QnCameraAdvancedParameter::setRange(int min, int max)
{
    NX_ASSERT(dataType == DataType::Number);
    range = lit("%1,%2").arg(min).arg(max);
}

void QnCameraAdvancedParameter::setRange(double min, double max)
{
    NX_ASSERT(dataType == DataType::Number);
    range = lit("%1,%2").arg(min).arg(max);
}

QnCameraAdvancedParameter QnCameraAdvancedParamGroup::getParameterById(const QString &id) const
{
    for (const QnCameraAdvancedParameter &param: params)
        if (param.id == id)
            return param;

    QnCameraAdvancedParameter result;
    for (const QnCameraAdvancedParamGroup &group: groups) {
        result = group.getParameterById(id);
        if (result.isValid())
            return result;
    }
    return result;
}

QSet<QString> QnCameraAdvancedParamGroup::allParameterIds() const
{
    QSet<QString> result;
    for (const QnCameraAdvancedParamGroup &subGroup: groups)
        result.unite(subGroup.allParameterIds());
    for (const QnCameraAdvancedParameter &param: params)
        if (!param.id.isEmpty())
            result.insert(param.id);
    return result;
}

bool QnCameraAdvancedParamGroup::updateParameter(const QnCameraAdvancedParameter &parameter)
{
    for (QnCameraAdvancedParameter &param: params) {
        if (param.id == parameter.id) {
            param = parameter;
            return true;
        }
    }
    for (QnCameraAdvancedParamGroup &group: groups) {
        if (group.updateParameter(parameter))
            return true;
    }
    return false;
}

QnCameraAdvancedParamGroup QnCameraAdvancedParamGroup::filtered(const QSet<QString> &allowedIds) const
{
    QnCameraAdvancedParamGroup result;
    result.name = this->name;
    result.description = this->description;

    for (const QnCameraAdvancedParamGroup &subGroup: groups) {
        QnCameraAdvancedParamGroup group = subGroup.filtered(allowedIds);
        if (!group.isEmpty())
            result.groups.push_back(group);
    }
    for (const QnCameraAdvancedParameter &param: params) {
        if (param.isValid() && allowedIds.contains(param.id))
            result.params.push_back(param);
    }
    return result;
}

bool QnCameraAdvancedParamGroup::isEmpty() const
{
    return params.empty() && groups.empty();
}

QnCameraAdvancedParameter QnCameraAdvancedParams::getParameterById(const QString &id) const
{
    QnCameraAdvancedParameter result;
    for (const QnCameraAdvancedParamGroup &group: groups) {
        result = group.getParameterById(id);
        if (result.isValid())
            return result;
    }
    return result;
}

QSet<QString> QnCameraAdvancedParams::allParameterIds() const
{
    QSet<QString> result;
    for (const QnCameraAdvancedParamGroup &group: groups)
        result.unite(group.allParameterIds());
    return result;
}

bool QnCameraAdvancedParams::updateParameter(const QnCameraAdvancedParameter &parameter)
{
    for (QnCameraAdvancedParamGroup &group: groups) {
        if (group.updateParameter(parameter))
            return true;
    }
    return false;
}

void QnCameraAdvancedParams::clear()
{
    groups.clear();
}

QnCameraAdvancedParams QnCameraAdvancedParams::filtered(const QSet<QString> &allowedIds) const
{
    QnCameraAdvancedParams result;
    result.name = this->name;
    result.unique_id = this->unique_id;
    result.version = this->version;
    result.packet_mode = this->packet_mode;
    for (const QnCameraAdvancedParamGroup &subGroup: groups) {
        QnCameraAdvancedParamGroup group = subGroup.filtered(allowedIds);
        if (!group.isEmpty())
            result.groups.push_back(group);
    }
    return result;
}

void QnCameraAdvancedParams::applyOverloads(const std::vector<QnCameraAdvancedParameterOverload>& overloads)
{
    if (overloads.empty())
        return;

    const QString kDefaultOverload = lit("default");
    std::map<QString, std::map<QString, QnCameraAdvancedParameterOverload>> overloadsMap;

    for (const auto& overload: overloads)
    {
        auto depId = overload.dependencyId.isEmpty() ? kDefaultOverload : overload.dependencyId;
        auto paramId = overload.paramId;
        overloadsMap[paramId][depId] = overload;
    }

    std::function<void(QnCameraAdvancedParamGroup& group)> traverseGroup;
    traverseGroup = [&kDefaultOverload, &overloadsMap, &traverseGroup] (QnCameraAdvancedParamGroup& group)
        {
            for (auto& param: group.params)
            {
                if (overloadsMap.find(param.id) == overloadsMap.end())
                    continue;

                auto paramOverload = overloadsMap[param.id];

                if (paramOverload.find(kDefaultOverload) != paramOverload.end())
                {
                    param.range = paramOverload[kDefaultOverload].range;
                    param.internalRange = paramOverload[kDefaultOverload].internalRange;
                }

                for (auto& dep: param.dependencies)
                {
                    if (dep.id.isEmpty())
                        continue;

                    if (paramOverload.find(dep.id) != paramOverload.end())
                    {
                        dep.range = paramOverload[dep.id].range;
                        dep.internalRange = paramOverload[dep.id].internalRange;
                    }
                }
            }

            for (auto& subgroup: group.groups)
                traverseGroup(subgroup);
        };

    for (auto& group: groups)
        traverseGroup(group);
}

static void mergeParams(
    std::vector<QnCameraAdvancedParameter>* target,
    std::vector<QnCameraAdvancedParameter>* source)
{
    for (auto& sourceParam: *source)
    {
        const auto targetParamById = std::find_if(
            target->begin(), target->end(),
            [&](const QnCameraAdvancedParameter& p){ return p.id == sourceParam.id; });

        if (targetParamById != target->end())
            NX_ASSERT(false, lm("Parameter with name '%1' clash").arg(sourceParam.id));
        else
            target->push_back(std::move(sourceParam));
    }
}

static void mergeGroups(
    std::vector<QnCameraAdvancedParamGroup>* target,
    std::vector<QnCameraAdvancedParamGroup>* source)
{
    for (auto& sourceGroup: *source)
    {
        const auto targetGroup = std::find_if(
            target->begin(), target->end(),
            [&](const QnCameraAdvancedParamGroup& g){ return g.name == sourceGroup.name; });

        if (targetGroup != target->end())
        {
            if (!targetGroup->description.isEmpty() && !sourceGroup.description.isEmpty())
                targetGroup->description += lit("\n") + sourceGroup.description;
            else
                targetGroup->description +=  sourceGroup.description;

            mergeParams(&targetGroup->params, &sourceGroup.params);
            mergeGroups(&targetGroup->groups, &sourceGroup.groups);
        }
        else
        {
            target->push_back(std::move(sourceGroup));
        }
    }
}

void QnCameraAdvancedParams::merge(QnCameraAdvancedParams params)
{
    // These fields are never used in code, but could still be usefull for debug.
    name += lit(", ") + params.name;
    version += lit(", ") + params.version;
    unique_id += lit(", ") + params.unique_id;
    packet_mode |= params.packet_mode;

    mergeGroups(&groups, &params.groups);
}

bool QnCameraAdvancedParameterCondition::checkValue(const QString& valueToCheck) const
{
    switch (type)
    {
        case ConditionType::equal:
            return value == valueToCheck;
        case ConditionType::inRange:
        {
            auto valuesList = value.split(L',');
            return valuesList.contains(valueToCheck);
        }
        case ConditionType::notInRange:
        {
            auto valuesList = value.split(L',');
            return !valuesList.contains(valueToCheck);
        }
        case ConditionType::valueChanged:
        {
            return true;
        }
        default:
        {
            NX_ASSERT(false, "We should never get here.");
            return false;
        }
    }
}

void QnCameraAdvancedParameterDependency::autoFillId()
{
    static const QChar kDelimiter(L',');
    QString hash = range + kDelimiter + internalRange;
    for (const auto& condition: conditions)
        hash += kDelimiter + condition.paramId + kDelimiter + condition.value;
    id = guidFromArbitraryData(hash).toSimpleString();
}

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnCameraAdvancedParameter, DataType,
    (QnCameraAdvancedParameter::DataType::Bool, "Bool")
    (QnCameraAdvancedParameter::DataType::String, "String")
    (QnCameraAdvancedParameter::DataType::Enumeration, "Enumeration")
    (QnCameraAdvancedParameter::DataType::Number, "Number")
    (QnCameraAdvancedParameter::DataType::Button, "Button")
    (QnCameraAdvancedParameter::DataType::Separator, "Separator")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnCameraAdvancedParameterCondition, ConditionType,
    (QnCameraAdvancedParameterCondition::ConditionType::equal, kEqualConditionType)
    (QnCameraAdvancedParameterCondition::ConditionType::inRange, kInRangeConditionType)
    (QnCameraAdvancedParameterCondition::ConditionType::notInRange, kNotInRangeConditionType)
    (QnCameraAdvancedParameterCondition::ConditionType::present, kPresenceConditionType)
    (QnCameraAdvancedParameterCondition::ConditionType::notPresent, kLackOfPresenceConditionType)
    (QnCameraAdvancedParameterCondition::ConditionType::valueChanged, kValueChangedConditionType)
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnCameraAdvancedParameterDependency, DependencyType,
    (QnCameraAdvancedParameterDependency::DependencyType::show, "Show")
    (QnCameraAdvancedParameterDependency::DependencyType::range, "Range")
    (QnCameraAdvancedParameterDependency::DependencyType::trigger, "Trigger")
)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(QnCameraAdvancedParameterTypes, (json), _Fields)
