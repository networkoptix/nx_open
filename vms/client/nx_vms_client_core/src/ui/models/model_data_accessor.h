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
    Q_INVOKABLE QString getHtmlEscapedData(const QModelIndex& index, const QString& roleName) const;

    Q_INVOKABLE bool setData(
        int row,
        int column,
        const QVariant& value,
        const QString& roleName) const;

    Q_INVOKABLE bool setHeaderData(
        int section,
        Qt::Orientation orientation,
        const QVariant& value,
        int role = Qt::EditRole);

signals:
    void rowsInserted(int first, int last); //< For now meaningful only for the flat list models.
    void rowsRemoved(int first, int last); //< For now meaningful only for the flat list models.
    void rowsMoved();
    void modelChanged();
    void countChanged();
    void dataChanged(int startRow, int endRow);

    // Implemented only for the horizontal header because we don't use vertical headers for now.
    void headerDataChanged(Qt::Orientation orientation, int first, int last);

private:
    QPointer<QAbstractItemModel> m_model;
};

} // namespace client
} // namespace nx
