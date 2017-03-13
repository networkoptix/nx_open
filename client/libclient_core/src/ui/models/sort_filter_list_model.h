#pragma once

#include <functional>

#include <QtCore/QAbstractListModel>
#include <QtCore/QScopedPointer>

class QAbstractListModel;

class QnSortFilterListModelPrivate;
class QnSortFilterListModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

public:
    QnSortFilterListModel(QObject* parent = nullptr);

    ~QnSortFilterListModel();

public:
    void setSourceModel(QAbstractListModel* model);
    QAbstractListModel* sourceModel() const;

    using SortingPredicate = std::function<bool (const QModelIndex& left,
        const QModelIndex& right)>;
    void setSortingPredicate(const SortingPredicate& pred);

    using FilteringPredicate = std::function<bool (const QModelIndex& value)>;
    void setFilteringPredicate(const FilteringPredicate& pred);

    typedef QList<int> RolesList;
    void setTriggeringRoles(const RolesList& roles);

    void forceUpdate();

public: // overrides section
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    Q_DECLARE_PRIVATE(QnSortFilterListModel)
    const QScopedPointer<QnSortFilterListModelPrivate> d_ptr;
};
