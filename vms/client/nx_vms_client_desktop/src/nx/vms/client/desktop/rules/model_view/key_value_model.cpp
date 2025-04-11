// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "key_value_model.h"

#include <QtQml/QtQml>

namespace nx::vms::client::desktop::rules {

KeyValueModel::KeyValueModel(QList<vms::rules::KeyValueObject> keyValueList, QObject* parent):
    QAbstractTableModel{parent},
    m_keyValueList{std::move(keyValueList)}
{
}

int KeyValueModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_keyValueList.size();
}

int KeyValueModel::columnCount(const QModelIndex& /*parent*/) const
{
    return ColumnCount;
}

QVariant KeyValueModel::data(const QModelIndex& index, int role) const
{
    if (role != Qt::DisplayRole || !index.isValid() || index.column() >= ColumnCount
        || index.row() >= m_keyValueList.size())
    {
        return {};
    }

    auto& item = m_keyValueList.at(index.row());
    return index.column() == KeyColumn ? item.key : item.value;
}

bool KeyValueModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || index.column() >= ColumnCount
        || index.row() >= m_keyValueList.size())
    {
        return false;
    }

    auto& item = m_keyValueList[index.row()];
    if (index.column() == KeyColumn)
        item.key = value.toString();
    else
        item.value = value.toString();

    emit dataChanged(index, index, {Qt::DisplayRole});

    return true;
}

Qt::ItemFlags KeyValueModel::flags(const QModelIndex& index) const
{
    return QAbstractTableModel::flags(index) | Qt::ItemIsSelectable | Qt::ItemIsEnabled
        | Qt::ItemIsEditable;
}

QVariant KeyValueModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Orientation::Horizontal)
        return {};

    switch (section)
    {
        case KeyColumn:
            return tr("Key");
        case ValueColumn:
            return tr("Value");
        default:
            return {};
    }
}

QHash<int, QByteArray> KeyValueModel::roleNames() const
{
    auto result = QAbstractTableModel::roleNames();
    result[Qt::EditRole] = "edit";
    return result;
}

bool KeyValueModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (row >= m_keyValueList.size() || row + count > m_keyValueList.size())
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    m_keyValueList.remove(row, count);
    endRemoveRows();

    return true;
}

void KeyValueModel::append(const QString& key, const QString& value)
{
    beginInsertRows({}, m_keyValueList.size(), m_keyValueList.size());
    m_keyValueList.append({key.trimmed(), value.trimmed()});
    endInsertRows();
}

const QList<vms::rules::KeyValueObject>& KeyValueModel::data() const
{
    return m_keyValueList;
}

void KeyValueModel::registerQmlType()
{
    qmlRegisterType<KeyValueModel>("nx.vms.client.desktop", 1, 0, "KeyValueModel");
}

} // namespace nx::vms::client::desktop::rules
