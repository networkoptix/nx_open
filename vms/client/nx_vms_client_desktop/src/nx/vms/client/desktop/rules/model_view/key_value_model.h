// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractTableModel>

#include <nx/vms/rules/field_types.h>

namespace nx::vms::client::desktop::rules {

class KeyValueModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Columns
    {
        KeyColumn,
        ValueColumn,
        ColumnCount
    };

    explicit KeyValueModel(QList<vms::rules::KeyValueObject>& keyValueList, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool removeRows(int row, int count, const QModelIndex& parent) override;

    Q_INVOKABLE void append(const QString& key, const QString& value);

    static void registerQmlType();

private:
    QList<vms::rules::KeyValueObject>& m_keyValueList;
};

} // namespace nx::vms::client::desktop::rules
