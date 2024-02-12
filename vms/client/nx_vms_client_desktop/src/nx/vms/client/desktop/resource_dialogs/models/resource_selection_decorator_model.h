// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QIdentityProxyModel>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>

namespace nx::vms::client::desktop {

/**
 * Proxy model that stores a set of resources considered as selected which provides correspondent
 * check state data accessible by <tt>Qt::CheckStateRole</tt> for given column.
 */
class ResourceSelectionDecoratorModel: public QIdentityProxyModel
{
    Q_OBJECT
    using base_type = QIdentityProxyModel;

public:
    /**
     * Proxy model which will additionally provide check state data accessible by the
     * <tt>Qt::CheckStateRole<tt> role at the last column. For resource rows this data corresponds
     * with the set of currently selected resources returned by <tt>selectedResources()</tt> method.
     * Check state data of grouping rows is deduced by the model.
     * @param selectionMode defines selection behavior, see <tt>ResourceSelectionMode</tt>.
     */
    ResourceSelectionDecoratorModel(
        ResourceSelectionMode selectionMode = ResourceSelectionMode::MultiSelection,
        QObject* parent = nullptr);

    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    /**
     * Flips check state provided by the index, if it provides non-null check state data. If
     * selection mode is MultiSelection than toggling check state of index that have checkable
     * children affects children check state accordingly. If selection mode is SingleSelection or
     * ExclusiveSelection than it's maintained by the model that not more than one or exactly one
     * index have checked state at the time.
     * @param index Valid index that belongs to this model expected.
     * @return True if check state is changed for at least one index as result of the operation.
     */
    bool toggleSelection(const QModelIndex& index);

    /**
     * If the selection mode is not ResourceSelectionMode::MultiSelection
     * or fromIndex and toIndex are not sibling indexes
     * or fromIndex and toIndex are the same index
     * then call of this function is equivalent to the toggleSelection(toIndex) call.
     *
     * Otherwise toggleSelection is called for toIndex and then called for every index in the range
     * [fromIndex, toIndex] that provides non-null check state that is different than toIndex
     * provides.
     *
     * @param fromIndex Valid index that belongs to this model expected.
     * @param toIndex Valid index that belongs to this model expected.
     * @return True if check state is changed for at least one index as result of the operation.
     */
    bool toggleSelection(const QModelIndex& fromIndex, const QModelIndex& toIndex);

    QSet<QnResourcePtr> selectedResources() const;
    void setSelectedResources(const QSet<QnResourcePtr>& resources);

    UuidSet selectedResourcesIds() const;

    ResourceSelectionMode selectionMode() const;
    void setSelectionMode(ResourceSelectionMode mode);

    QModelIndex resourceIndex(const QnResourcePtr& resource) const;

signals:
    void selectedResourcesChanged();

private:
    QSet<QnResourcePtr> m_selectedResources;
    ResourceSelectionMode m_resourceSelectionMode;
    QHash<QnResourcePtr, QPersistentModelIndex> m_resourceMapping;
};

} // namespace nx::vms::client::desktop
