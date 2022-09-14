// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>

class QnResourcePool;

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class UserLayoutResourceIndex: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    UserLayoutResourceIndex(const QnResourcePool* resourcePool);

    QVector<QnResourcePtr> layoutsWithParentUserId(const QnUuid& parentId) const;

signals:
    void layoutAdded(const QnResourcePtr& layout, const QnUuid& parentId);
    void layoutRemoved(const QnResourcePtr& layout, const QnUuid& parentId);

private:
    void onResourceAdded(const QnResourcePtr& resource);
    void onResourceRemoved(const QnResourcePtr& resource);
    void onLayoutParentIdChanged(const QnResourcePtr& resource, const QnUuid& previousParentId);

private:
    void indexAllLayouts();
    void indexLayout(const QnResourcePtr& layout);

private:
    const QnResourcePool* m_resourcePool;
    QHash<QnUuid, QSet<QnResourcePtr>> m_layoutsByParentUserId;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
