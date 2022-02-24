// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QAbstractProxyModel>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_model_operations.h>

namespace nx::vms::client::desktop {

/**
 * A replacement for QSortFilterProxyModel if only a filtering of rows is required.
 *
 * Source row filtering predicate can be defined as a lambda passed to `setFilter` function,
 * or at a lower level by overriding `filterAcceptsRow` protected function. The predicate may
 * depend on a row itself, its parents and some external conditions, but not on its siblings.
 * If external conditions change, `invalidateFilter` function should be called to re-filter
 * the entire model.
 *
 * Unlike QSortFilterProxyModel which translates all row moves into layout changes,
 * this model translates source row moves into proxy row moves, insertions or deletions.
 * If source rows are moved within the same parent, the filtered state of them is not recomputed
 * (that is the reason why the filter predicate must not depend on row's siblings).
 * If source rows are moved between different parents, the filtered state of them is recomputed
 * only if `recalculateFilterAfterLongMove` is set to true; in this case if the filtered state
 * changes after recomputation, the initial proxy row move, insertion or deletion is followed
 * by a proxy layout change of the destination parent.
 */
class NX_VMS_CLIENT_DESKTOP_API FilterProxyModel:
    public ScopedModelOperations<QAbstractProxyModel>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractProxyModel>;

public:
    explicit FilterProxyModel(QObject* parent = nullptr);
    virtual ~FilterProxyModel() override;

    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const;
    virtual QModelIndex mapToSource(const QModelIndex& index) const;

    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex index(
        int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex& index) const override;
    virtual bool hasChildren(const QModelIndex& parent) const override;

    virtual void setSourceModel(QAbstractItemModel* value) override;

    /** Filtering predicate used by the default implementation of `filterAcceptsRow` */
    using IsRowAccepted = std::function<bool(int sourceRow, const QModelIndex& sourceParent)>;
    IsRowAccepted filter() const;
    void setFilter(IsRowAccepted value);

    /** Re-filter the entire model. */
    void invalidateFilter();

    /** Whether to automatically recalculate filter for rows moved between different parents. */
    bool recalculateFilterAfterLongMove() const;
    void setRecalculateFilterAfterLongMove(bool value);

    bool debugChecksEnabled() const;
    void setDebugChecksEnabled(bool value);

protected:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
