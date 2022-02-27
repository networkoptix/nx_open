// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QIdentityProxyModel>

#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/resource_dialogs/resource_selection_dialogs_globals.h>

namespace nx::vms::client::desktop {

class ResourceSelectionDecoratorModel: public QIdentityProxyModel
{
    Q_OBJECT
    using base_type = QIdentityProxyModel;

public:
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
