#pragma once

#include <functional>

#include <QtCore/QAbstractProxyModel>
#include <QtCore/QScopedPointer>

class QAbstractListModel;

class QnSortFilterListModelPrivate;
class QnSortFilterListModel: public QAbstractProxyModel
{
    Q_OBJECT
    using base_type = QAbstractProxyModel;

public:
    QnSortFilterListModel(QObject* parent = nullptr);

    ~QnSortFilterListModel();

public:
    using RolesSet = QSet<int>;
    void setTriggeringRoles(const RolesSet& roles);

    void forceUpdate();

public:
    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const;

    virtual bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const;

public: // overrides section
    virtual void setSourceModel(QAbstractItemModel* model);

    virtual QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
    virtual QModelIndex index(
        int row,
        int column,
        const QModelIndex& parent = QModelIndex()) const override;

    virtual QModelIndex parent(const QModelIndex& child) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

private:
    Q_DECLARE_PRIVATE(QnSortFilterListModel)
    const QScopedPointer<QnSortFilterListModelPrivate> d_ptr;
};
