// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/scoped_model_operations.h>

namespace nx::vms::client::desktop {

/**
 * A simple item model wrapper for an index list from another model.
 */
class IndexListModel: public ScopedModelOperations<QAbstractListModel>
{
    Q_OBJECT
    Q_PROPERTY(QModelIndexList source READ source WRITE setSource NOTIFY sourceChanged)

    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    using base_type::base_type; //< Forward constructors.

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QModelIndex sourceIndex(int row) const;

    QModelIndexList source() const;
    void setSource(const QModelIndexList& value);

    static void registerQmlType();

signals:
    void sourceChanged(QPrivateSignal);

private:
    QList<QPersistentModelIndex> m_source;
    QHash<int, QByteArray> m_roleNames;
};

} // namespace nx::vms::client::desktop
