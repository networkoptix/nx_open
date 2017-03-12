#pragma once

#include <functional>

#include <QtCore/QAbstractListModel>
#include <QtCore/QScopedPointer>

class QAbstractListModel;

class QnSortingProxyModelPrivate;
class QnSortingProxyModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

public:
    QnSortingProxyModel(QObject* parent = nullptr);

    ~QnSortingProxyModel();

public:
    void setSourceModel(QAbstractListModel* model);
    QAbstractListModel* sourceModel() const;

    typedef std::function<bool (const QModelIndex& left,
        const QModelIndex& right)> SortingPredicate;
    void setSortingPred(const SortingPredicate& pred);

    typedef std::function<bool (const QModelIndex& value)> FilteringPredicate;
    void setFilteringPred(const FilteringPredicate& pred);

    typedef QList<int> RolesList;
    void setTriggeringRoles(const RolesList& roles);

    void forceUpdate();

public: // overrides section
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    Q_DECLARE_PRIVATE(QnSortingProxyModel)
    const QScopedPointer<QnSortingProxyModelPrivate> d_ptr;
};
