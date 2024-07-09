// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_entries_model_p.h"

#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find_if.hpp>

#include <QtGui/QColor>

#include <nx/reflect/json/deserializer.h>
#include <nx/vms/client/core/analytics/taxonomy/attribute_set.h>
#include <nx/vms/client/core/analytics/taxonomy/color_set.h>
#include <nx/vms/client/core/analytics/taxonomy/enumeration.h>
#include <nx/vms/client/core/analytics/taxonomy/object_type.h>
#include <utils/common/hash.h>

namespace nx::vms::client::desktop {

QVariant LookupListEntriesModel::Private::intFormatter(const QString& value)
{
    auto [result, ok] = nx::reflect::json::deserialize<int>(value.toStdString());
    return result;
}

QVariant LookupListEntriesModel::Private::doubleFormatter(const QString& value)
{
    auto [result, ok] = nx::reflect::json::deserialize<double>(value.toStdString());
    return result;
}

QVariant LookupListEntriesModel::Private::stringFormatter(const QString& value)
{
    return value;
}

QVariant LookupListEntriesModel::Private::booleanFormatter(const QString& value)
{
    if (value == "true")
        return tr("Yes");
    if (value == "false")
        return tr("No");

    NX_ASSERT(false, "Unexpected value");
    return {};
}

QVariant LookupListEntriesModel::Private::objectFormatter(const QString& value)
{
    if (value == "true")
        return tr("Present");
    if (value == "false")
        return tr("Absent");

    NX_ASSERT(false, "Unexpected value");
    return {};
}

QVariant LookupListEntriesModel::Private::getDisplayValue(
    const QString& attributeName, const QString& value) const
{
    if (data->rawData().objectTypeId.isEmpty())
        return value; //< Generic Model.

    auto it = formatterByAttributeName.find(attributeName);
    if (!NX_ASSERT(it != formatterByAttributeName.end(), "Incorrect attribute name"))
        return {};

    if (value.isEmpty())
        return tr("Any %1").arg(attributeName.split(".").back());

    return it->second(value);
}

bool LookupListEntriesModel::Private::intValidator(
    const QString& value, const QVariant& min, const QVariant& max)
{
    bool ok = false;
    //TODO: change to reflect::json::deserialize after fix of VMS-53821
    int intValue = value.toInt(&ok);
    if (!ok)
        return false;
    if (!min.isNull() && min.toInt() > intValue)
        return false;
    if (!max.isNull() && max.toInt() < intValue)
        return false;
    return true;
}

bool LookupListEntriesModel::Private::doubleValidator(
    const QString& value, const QVariant& min, const QVariant& max)
{
    double doubleValue;
    if (!reflect::json::deserialize(value.toStdString(), &doubleValue))
        return false;
    if (!min.isNull() && min.toDouble() > doubleValue)
        return false;
    if (!max.isNull() && max.toDouble() < doubleValue)
        return false;
    return true;
}

bool LookupListEntriesModel::Private::booleanValidator(const QString& value)
{
    bool ignored;
    return reflect::json::deserialize(value.toStdString(), &ignored);
}

void LookupListEntriesModel::Private::initAttributeFunctions()
{
    using namespace nx::vms::client::core::analytics::taxonomy;

    if (!taxonomy)
        return;

    validatorByAttributeName.clear();
    if (!data)
        return;

    const ObjectType* objectType =
        taxonomy->objectTypeById(data->rawData().objectTypeId);

    if (objectType == nullptr)
        return;

    std::function<void(const std::vector<Attribute*>&, const QString&)>
        collectAttributesValuesRecursive =
            [&](const std::vector<Attribute*>& attributes, const QString& parentAttributeName)
    {
        for (const auto& attribute: attributes)
        {
            const QString fullAttributeName = parentAttributeName.isEmpty()
                ? attribute->name
                : parentAttributeName + "." + attribute->name;

            formatterByAttributeName[fullAttributeName] = &Private::stringFormatter;
            switch (attribute->type)
            {
                case Attribute::Type::number:
                {
                    const bool isIntType = attribute->subtype == "integer";
                    formatterByAttributeName[fullAttributeName] = isIntType
                        ? &Private::intFormatter
                        : &Private::doubleFormatter;
                    validatorByAttributeName[fullAttributeName] =
                        [isIntType, min = attribute->minValue, max = attribute->maxValue]
                        (const QString& value)
                        {
                            return isIntType ? intValidator(value, min, max)
                                             : doubleValidator(value, min, max);
                        };
                    break;
                }
                case Attribute::Type::boolean:
                {
                    validatorByAttributeName[fullAttributeName] = &Private::booleanValidator;
                    formatterByAttributeName[fullAttributeName] = &Private::booleanFormatter;
                    break;
                }
                case Attribute::Type::attributeSet:
                {
                    validatorByAttributeName[fullAttributeName] = &Private::booleanValidator;
                    formatterByAttributeName[fullAttributeName] = &Private::objectFormatter;
                    collectAttributesValuesRecursive(
                        attribute->attributeSet->attributes(), fullAttributeName);
                    break;
                }
                case Attribute::Type::colorSet:
                {
                    const auto colorByHex = attribute->colorSet->colorByHex();

                    validatorByAttributeName[fullAttributeName] =
                        [colorByHex](const QString& value)
                    {
                        return ranges::any_of(colorByHex,
                            [value](const auto& keyValue)
                            {
                                return keyValue.first == value || keyValue.second == value;
                            });
                    };

                    formatterByAttributeName[fullAttributeName] =
                        [colorByHex](const QString& value)
                    {
                        auto colorIt = ranges::find_if(colorByHex,
                            [value](const auto& keyValue)
                            {
                                return keyValue.first == value || keyValue.second == value;
                            });
                        if (!NX_ASSERT(colorIt != colorByHex.cend(), "Unexpected value"))
                            return QString();
                        return colorIt->second;
                    };
                    break;
                }
                case Attribute::Type::enumeration:
                {
                    const auto items = attribute->enumeration->items();
                    const QSet itemSet(items.begin(), items.end());
                    validatorByAttributeName[fullAttributeName] = [itemSet](const QString& value)
                    { return itemSet.contains(value); };
                    break;
                }
                default:
                {
                    // String and undefined values.
                    validatorByAttributeName[fullAttributeName] =
                        [](const QString&) { return true; };
                }
            }
        }
    };

    collectAttributesValuesRecursive(objectType->attributes(), {});
}

QVector<int> LookupListEntriesModel::Private::removeIncorrectColumnValues()
{
    auto& lookuplistData = data->rawData();

    QVector<int> emptyRowsToDelete;
    for (int rowIndex = 0; rowIndex < lookuplistData.entries.size(); ++rowIndex)
    {
        auto& row = lookuplistData.entries[rowIndex];
        if (row.empty())
        {
            emptyRowsToDelete.push_back(rowIndex);
            continue;
        }

        // Remove invalid column value.
        std::erase_if(
            row,
            [&](const auto& entry) { return !isValidValue(entry.second, entry.first); });

        const bool hasAtLeastOneValidValue = ranges::any_of(
            row,
            [&](const auto& entry)
            {
                return !entry.second.isEmpty() && isValidValue(entry.second, entry.first);
            });

        if (!hasAtLeastOneValidValue)
            emptyRowsToDelete.push_back(rowIndex);
    }
    return emptyRowsToDelete;
}

bool LookupListEntriesModel::Private::isValidValue(const QString& value, const QString& attributeName) const
{
    if (validatorByAttributeName.isEmpty())
        return true; //< No validation is required.

    const auto& displayedAttributes = data->rawData().attributeNames;

    if (!ranges::contains(displayedAttributes, attributeName))
        return false; //< There is no such attribute name in displayed columns.

    if (value.isEmpty())
        return true;

    if (const auto validator = validatorByAttributeName.value(attributeName))
        return validator(value);

    return false; //< There is no such attribute name in ObjectTypeId.
}

} // namespace nx::vms::client::desktop
