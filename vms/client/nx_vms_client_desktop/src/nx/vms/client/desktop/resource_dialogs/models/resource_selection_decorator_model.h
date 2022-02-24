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
        ResourceSelectionMode selectionMode = ResourceSelectionMode::MultiSelection);

    virtual QVariant data(const QModelIndex& index, int role) const override;

    bool toggleSelection(const QModelIndex& index);

    QSet<QnResourcePtr> selectedResources() const;
    void setSelectedResources(const QSet<QnResourcePtr>& resources);

    QnUuidSet selectedResourcesIds() const;

    ResourceSelectionMode selectionMode() const;
    void setSelectionMode(ResourceSelectionMode mode);

private:
    QSet<QnResourcePtr> m_selectedResources;
    ResourceSelectionMode m_resourceSelectionMode;
    QHash<QnResourcePtr, QPersistentModelIndex> m_resourceMapping;
};

} // namespace nx::vms::client::desktop
