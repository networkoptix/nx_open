// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>

class QnResourcePool;

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class WebPageResourceIndex: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    WebPageResourceIndex(const QnResourcePool* resourcePool);

    QVector<QnResourcePtr> allProxiedWebResources() const;
    QVector<QnResourcePtr> webPagesOnServer(const QnUuid& serverId) const;
    QVector<QnResourcePtr> allWebPages() const;

signals:
    void webPageAdded(const QnResourcePtr& webPage);
    void webPageRemoved(const QnResourcePtr& webPage);

    void webPageAddedToServer(const QnResourcePtr& webPage, const QnResourcePtr& server);
    void webPageRemovedFromServer(const QnResourcePtr& webPage, const QnResourcePtr& server);

private:
    void onResourceAdded(const QnResourcePtr& resource);
    void onResourceRemoved(const QnResourcePtr& resource);
    void onWebPageParentIdChanged(const QnResourcePtr& webPage, const QnUuid& previousParentId);

private:
    void indexAllWebPages();
    void indexWebPage(const QnWebPageResourcePtr& webPage);

private:
    const QnResourcePool* m_resourcePool;
    QHash<QnUuid, QSet<QnResourcePtr>> m_webPagesByServer;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
