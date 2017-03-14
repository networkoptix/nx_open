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
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    Q_DECLARE_PRIVATE(QnSortFilterListModel)
    const QScopedPointer<QnSortFilterListModelPrivate> d_ptr;
};
