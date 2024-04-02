// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_entries_model.h"

#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/view/reverse.hpp>

#include <QtCore/QString>

#include <nx/reflect/json/deserializer.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/desktop/analytics/taxonomy/attribute_set.h>
#include <nx/vms/client/desktop/analytics/taxonomy/color_set.h>
#include <nx/vms/client/desktop/analytics/taxonomy/enumeration.h>
#include <nx/vms/client/desktop/analytics/taxonomy/object_type.h>
#include <ui/utils/table_export_helper.h>
#include <utils/common/hash.h>

namespace nx::vms::client::desktop {

namespace {

QString attribute(const nx::vms::api::LookupListData& list, int column)
{
    if (column >= list.attributeNames.size())
        return {};

    return list.attributeNames[column];
}

} // namespace

class LookupListEntriesModel::Private
{
public:
    using Validator = std::function<bool(const QString&)>;

    QPointer<LookupListModel> data;
    std::optional<QList<int>> rowsIndexesToShow;
    QMap<QString, Validator> validatorByAttributeName;
    analytics::taxonomy::StateView* taxonomy;
    void initValidators();
    static bool numberValidator(const QString&);
    static bool booleanValidator(const QString&);
};

bool LookupListEntriesModel::Private::numberValidator(const QString& value)
{
    int ignored;
    double ignoredDouble;
    const std::string stdString(value.toStdString());
    return reflect::json::deserialize(stdString, &ignored)
        || reflect::json::deserialize(stdString, &ignoredDouble);
}

bool LookupListEntriesModel::Private::booleanValidator(const QString& value)
{
    bool ignored;
    return reflect::json::deserialize(value.toStdString(), &ignored);
}

void LookupListEntriesModel::Private::initValidators()
{
   using namespace analytics::taxonomy;

    if (!taxonomy)
        return;

    validatorByAttributeName.clear();
    if (!data)
        return;

    const ObjectType* objectType = taxonomy->objectTypeById(data->rawData().objectTypeId);

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

            switch (attribute->type)
            {
                case Attribute::Type::number:
                {
                    validatorByAttributeName[fullAttributeName] = &Private::numberValidator;
                    break;
                }
                case Attribute::Type::boolean:
                {
                    validatorByAttributeName[fullAttributeName] = &Private::booleanValidator;
                    break;
                }
                case Attribute::Type::attributeSet:
                {
                    validatorByAttributeName[fullAttributeName] = &Private::booleanValidator;
                    collectAttributesValuesRecursive(
                        attribute->attributeSet->attributes(), fullAttributeName);
                    break;
                }
                case Attribute::Type::colorSet:
                {
                    QSet<QColor> itemSet;
                    for (const auto& item: attribute->colorSet->items())
                        itemSet.insert(QColor::fromString(item));

                    validatorByAttributeName[fullAttributeName] =
                        [itemSet](const QString& value)
                        {
                            const auto color = QColor::fromString(value);
                            return color.isValid() && itemSet.contains(color);
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

LookupListEntriesModel::LookupListEntriesModel(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

LookupListEntriesModel::~LookupListEntriesModel()
{
}

QVariant LookupListEntriesModel::headerData(
    int section, Qt::Orientation orientation, int role) const
{
    if (!NX_ASSERT(d->data))
        return {};

    if (orientation != Qt::Orientation::Horizontal)
        return QVariant();

    if (role == Qt::DisplayRole)
        return attribute(d->data->rawData(), section);

    return QVariant();
}

int LookupListEntriesModel::rowCount(const QModelIndex& /*parent*/) const
{
    if (!d->data)
        return 0;

    return d->rowsIndexesToShow ? d->rowsIndexesToShow->size() : d->data->rawData().entries.size();
}

int LookupListEntriesModel::columnCount(const QModelIndex& parent) const
{
    // Reserve one for checkbox column.
    return d->data ? (int) d->data->rawData().attributeNames.size() : 0;
}

QVariant LookupListEntriesModel::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(d->data))
        return {};

    switch (role)
    {
        case Qt::DisplayRole:
        {
            const auto key = attribute(d->data->rawData(), index.column());
            if (key.isEmpty())
                return QString();

            const int rowIdx =
                d->rowsIndexesToShow ? d->rowsIndexesToShow->at(index.row()) : index.row();
            const auto& entry = d->data->rawData().entries[rowIdx];
            const auto iter = entry.find(key);
            if (iter != entry.cend())
                return iter->second;
            return QString();
        }

        case ObjectTypeIdRole:
        {
            return d->data->rawData().objectTypeId;
        }

        case AttributeNameRole:
        {
            return attribute(d->data->rawData(), index.column());
        }
    }

    return {};
}

bool LookupListEntriesModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!NX_ASSERT(d->data))
        return false;
    if (role != Qt::EditRole)
        return false;


    const auto key = attribute(d->data->rawData(), index.column());
    if (key.isEmpty())
        return false;

    auto& entry = d->data->rawData().entries[index.row()];
    auto iter = entry.find(key);

    const auto valueAsString = value.toString();
    if (valueAsString.isEmpty() && iter != entry.end())
        entry.erase(iter);
    else
        entry[key] = valueAsString;

    emit dataChanged(index, index, {Qt::DisplayRole});
    return true;
}

QHash<int, QByteArray> LookupListEntriesModel::roleNames() const
{
    auto roles = base_type::roleNames();
    roles[ObjectTypeIdRole] = "objectTypeId";
    roles[AttributeNameRole] = "attributeName";
    return roles;
}

LookupListModel* LookupListEntriesModel::listModel() const
{
    return d->data.data();
}

void LookupListEntriesModel::setListModel(LookupListModel* value)
{
    beginResetModel();
    if (d->data)
        d->data->disconnect(this);

    d->data = value;
    endResetModel();
    d->initValidators();
    emit listModelChanged(value);

    if (d->data)
    {
        connect(d->data.get(),
            &LookupListModel::attributeNamesChanged,
            this,
            &LookupListEntriesModel::removeIncorrectEntries);
        emit headerDataChanged(Qt::Orientation::Horizontal, 0, columnCount() - 1);
        emit rowCountChanged();
    }
}

void LookupListEntriesModel::addEntry(const QVariantMap& values)
{
    if (!NX_ASSERT(d->data))
        return;

    if (!NX_ASSERT(!values.empty()))
        return;

    const int count = d->data->rawData().entries.size();
    beginInsertRows({}, count, count);

    nx::vms::api::LookupListEntry entry;
    for (const auto& [name, value]: nx::utils::constKeyValueRange(values))
        entry[name] = value.toString();
    d->data->rawData().entries.push_back(std::move(entry));

    endInsertRows();
    emit rowCountChanged();
}

void LookupListEntriesModel::deleteEntries(const QVector<int>& rows)
{
    if (!NX_ASSERT(d->data))
        return;

    if (rows.empty())
        return;

    const std::set<int, std::greater<>> uniqueValuesInDescendingOrder(rows.begin(), rows.end());
    beginResetModel();
    for (const auto& index: uniqueValuesInDescendingOrder)
    {
        if (index >= d->data->rawData().entries.size())
            continue;
        d->data->rawData().entries.erase(d->data->rawData().entries.begin() + index);
    }
    endResetModel();
    emit rowCountChanged();
}

void LookupListEntriesModel::exportEntries(const QSet<int>& selectedRows, QTextStream& outputCsv)
{
    if (!NX_ASSERT(d->data))
        return;

    QModelIndexList indexes;
    if (selectedRows.isEmpty() || selectedRows.size() == rowCount())
    {
        indexes = QnTableExportHelper::getAllIndexes(this);
    }
    else
    {
        indexes = QnTableExportHelper::getFilteredIndexes(
            this, [&](const QModelIndex& index) { return selectedRows.contains(index.row()); });
    }

    QnTableExportHelper::exportToStreamCsv(this, indexes, outputCsv);
}

bool LookupListEntriesModel::isValidValue(const QString& value, const QString& attributeName)
{
    if (d->validatorByAttributeName.isEmpty())
        return true; //< No validation is required.

    const auto& displayedAttributes = d->data->rawData().attributeNames;

    if (!ranges::contains(displayedAttributes, attributeName))
        return false; //< There is no such attribute name in displayed columns.

    if (value.isEmpty())
        return true;

    if (const auto validator = d->validatorByAttributeName.value(attributeName))
        return validator(value);

    return false; //< There is no such attribute name in ObjectTypeId.
}

analytics::taxonomy::StateView* LookupListEntriesModel::taxonomy()
{
    return d->taxonomy;
}

void LookupListEntriesModel::setTaxonomy(analytics::taxonomy::StateView* taxonomy)
{
    d->taxonomy = taxonomy;
    d->initValidators();
    emit taxonomyChanged();
}

int LookupListEntriesModel::columnPosOfAttribute(const QString& attributeName)
{
    if (!d->data)
        return -1;

    const auto& attributeNames = d->data->rawData().attributeNames;
    const auto pos = std::find(attributeNames.begin(), attributeNames.end(), attributeName);
    return pos != attributeNames.end() ? std::distance(attributeNames.begin(), pos) : -1;
}

void LookupListEntriesModel::setFilter(const QString& searchText, int resultLimit)
{
    if (!NX_ASSERT(d->data))
        return;

    beginResetModel();
    if (searchText.isEmpty())
        d->rowsIndexesToShow.reset();
    else
        d->rowsIndexesToShow = d->data->setFilter(searchText, resultLimit);
    endResetModel();
}

void LookupListEntriesModel::removeIncorrectEntries()
{
    if (!NX_ASSERT(d->data, "Function must never be called on uninitialized model"))
        return;

    auto& lookuplistData = d->data->rawData();
    QVector<int> emptyRowsToDelete;
    for (int rowIndex = 0; rowIndex < lookuplistData.entries.size(); ++rowIndex)
    {
        auto& row = lookuplistData.entries[rowIndex];
        if (row.empty())
        {
            emptyRowsToDelete.push_back(rowIndex);
            continue;
        }

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

    // Remove empty rows.
    deleteEntries(emptyRowsToDelete);
}

void LookupListEntriesModel::update()
{
    beginResetModel();
    endResetModel();
    emit rowCountChanged();
}

} // namespace nx::vms::client::desktop
