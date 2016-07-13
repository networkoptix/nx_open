#pragma once

#include <QtCore/QSortFilterProxyModel>

class QnQmlSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    typedef QSortFilterProxyModel base_type;

    Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel NOTIFY modelChanged)

public:
    QnQmlSortFilterProxyModel();

    QnQmlSortFilterProxyModel(QObject* parent);

    virtual ~QnQmlSortFilterProxyModel() = default;

public:
    QAbstractItemModel* model() const;

    void setModel(QAbstractItemModel* targetModel);

signals:
    void modelChanged();
};