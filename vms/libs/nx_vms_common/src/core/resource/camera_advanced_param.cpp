// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_advanced_param.h"

#include <boost/range.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/range/algorithm/find_if.hpp>

#include <core/resource/resource.h>
#include <core/resource/resource_property_key.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

using ConditionType = QnCameraAdvancedParameterCondition::ConditionType;
using DependencyType = QnCameraAdvancedParameterDependency::DependencyType;

QnCameraAdvancedParamValue::QnCameraAdvancedParamValue(const QString &id, const QString &value):
    id(id), value(value)
{
}

QString QnCameraAdvancedParamValue::toString() const
{
    return nx::format("%1 = %2").args(id, value);
}

//--------------------------------------------------------------------------------------------------
// QnCameraAdvancedParamValueMapHelper

QnCameraAdvancedParamValueMapHelper::QnCameraAdvancedParamValueMapHelper(
    const QnCameraAdvancedParamValueMap& map)
    :
    m_map(map)
{
}

QnCameraAdvancedParamValueMap QnCameraAdvancedParamValueMapHelper::makeMap(
    const QnCameraAdvancedParamValueList& list)
{
    QnCameraAdvancedParamValueMap result;
    return QnCameraAdvancedParamValueMapHelper(result).appendValueList(list);
}

QnCameraAdvancedParamValueMap  QnCameraAdvancedParamValueMapHelper::makeMap(
    std::initializer_list<std::pair<QString, QString>> values)
{
    QnCameraAdvancedParamValueMap result;
    for (const auto& v: values)
        result.insert(v.first, v.second);
    return result;
}

QnCameraAdvancedParamValueList QnCameraAdvancedParamValueMapHelper::toValueList() const
{
    QnCameraAdvancedParamValueList result;
    for (auto iter = m_map.cbegin(); iter != m_map.cend(); ++iter)
        result << QnCameraAdvancedParamValue(iter.key(), iter.value());
    return result;
}

QnCameraAdvancedParamValueMap QnCameraAdvancedParamValueMapHelper::appendValueList(
    const QnCameraAdvancedParamValueList& list)
{
    QnCameraAdvancedParamValueMap result = m_map;
    for (const QnCameraAdvancedParamValue& value: list)
        result.insert(value.id, value.value);
    return result;
}

//--------------------------------------------------------------------------------------------------
// QnCameraAdvancedParameter

bool QnCameraAdvancedParameter::isValid() const
{
    return (dataType != DataType::None)
        && (!id.isEmpty());
}

bool QnCameraAdvancedParameter::isCustomControl() const
{
    return isValid() && writeCmd.startsWith(lit("custom_"));
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

bool QnCameraAdvancedParameter::hasValue() const
{
    return isValid() && dataTypeHasValue(dataType);
}

bool QnCameraAdvancedParameter::isInstant() const
{
    return isValid() && !dataTypeHasValue(dataType);
}

bool QnCameraAdvancedParameter::dataTypeHasValue(DataType value)
{
    switch (value)
    {
        case DataType::Bool:
        case DataType::Number:
        case DataType::Enumeration:
        case DataType::String:
        // Some 3rd-party plugins can not properly provide any value for these controls.
        case DataType::SliderControl:
        case DataType::PtrControl:
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
    return range.split(',', Qt::SkipEmptyParts);
}

QStringList QnCameraAdvancedParameter::getInternalRange() const
{
    NX_ASSERT(dataType == DataType::Enumeration);
    return internalRange.split(',', Qt::SkipEmptyParts);
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
    QStringList values = range.split(',');
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
    result.pluginUniqueId = this->pluginUniqueId;
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
            NX_ASSERT(false, nx::format("Parameter with name '%1' clash").arg(sourceParam.id));
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
    pluginUniqueId += lit(", ") + params.pluginUniqueId;
    packet_mode |= params.packet_mode;

    mergeGroups(&groups, &params.groups);
}

bool QnCameraAdvancedParams::hasItemsAvailableInOffline() const
{
    std::function<bool(const QnCameraAdvancedParamGroup&)> hasItemsAvailableInOfflineInGroup;
    hasItemsAvailableInOfflineInGroup =
        [&](const QnCameraAdvancedParamGroup& group)
        {
            for (const QnCameraAdvancedParameter& param: group.params)
            {
                if (param.availableInOffline)
                    return true;
            }

            for (const QnCameraAdvancedParamGroup& subgroup: group.groups)
            {
                if (hasItemsAvailableInOfflineInGroup(subgroup))
                    return true;
            }

            return false;
        };

    for (const QnCameraAdvancedParamGroup& group: groups)
    {
        if (hasItemsAvailableInOfflineInGroup(group))
            return true;
    }

    return false;
}

QnCameraAdvancedParameterCondition::QnCameraAdvancedParameterCondition(
    QnCameraAdvancedParameterCondition::ConditionType type,
    const QString& paramId,
    const QString& value)
    :
    type(type),
    paramId(paramId),
    value(value)
{
}

bool QnCameraAdvancedParameterCondition::checkValue(const QString& valueToCheck) const
{
    switch (type)
    {
        case ConditionType::equal:
            return value == valueToCheck;

        case ConditionType::notEqual:
            return value != valueToCheck;

        case ConditionType::inRange:
            return value.split(',').contains(valueToCheck);

        case ConditionType::notInRange:
            return !value.split(',').contains(valueToCheck);

        case ConditionType::valueChanged:
            return true;

        case ConditionType::contains:
            return valueToCheck.contains(value);

        case ConditionType::unknown:
            NX_ASSERT(false, "Unexpected unknown condition");
            return false;
    }

    NX_ASSERT(false, nx::format("Unexpected condition value: %1").args(static_cast<int>(type)));
    return false;
}

void QnCameraAdvancedParameterDependency::autoFillId(const QString& prefix)
{
    static const QChar kDelimiter(',');
    QString hash = range + kDelimiter + internalRange;
    for (const auto& condition: conditions)
        hash += kDelimiter + condition.paramId + kDelimiter + condition.value;
    id = prefix.isEmpty()
        ? QnUuid::fromArbitraryData(hash).toSimpleString()
        : prefix + "_" + QnUuid::fromArbitraryData(hash).toSimpleString();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCameraAdvancedParamValue, (json), QnCameraAdvancedParamValue_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCameraAdvancedParameter, (json), QnCameraAdvancedParameter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCameraAdvancedParamGroup, (json), QnCameraAdvancedParamGroup_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraAdvancedParams, (json), QnCameraAdvancedParams_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCameraAdvancedParameterDependency, (json), QnCameraAdvancedParameterDependency_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCameraAdvancedParameterCondition, (json), QnCameraAdvancedParameterCondition_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCameraAdvancedParameterOverload, (json), QnCameraAdvancedParameterOverload_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCameraAdvancedParamsPostBody, (json), QnCameraAdvancedParamsPostBody_Fields)
