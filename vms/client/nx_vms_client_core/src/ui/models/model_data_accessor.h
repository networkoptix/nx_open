// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QPointer>

namespace nx {
namespace client {

class ModelDataAccessor: public QObject
{
    using base_type = QObject;

    Q_OBJECT
    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    ModelDataAccessor(QObject* parent = nullptr);
    ~ModelDataAccessor();

    QVariant model() const;
    void setModel(const QVariant& modelVariant);

    int count() const;

    Q_INVOKABLE QVariant getData(int row, const QString& roleName) const;
    Q_INVOKABLE QVariant getData(const QModelIndex& index, const QString& roleName) const;

signals:
    void rowsInserted(int first, int last); //< For now meaningful only for the flat list models.
    void rowsRemoved(int first, int last); //< For now meaningful only for the flat list models.
    void rowsMoved();
    void modelChanged();
    void countChanged();
    void dataChanged(int startRow, int endRow);

private:
    QPointer<QAbstractItemModel> m_model;
};

} // namespace client
} // namespace nx
