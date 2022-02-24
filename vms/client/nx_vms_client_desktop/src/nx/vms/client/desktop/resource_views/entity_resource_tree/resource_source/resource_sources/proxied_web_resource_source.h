// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

class QnResourcePool;

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class WebPageResourceIndex;

class ProxiedWebResourceSource: public AbstractResourceSource
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    ProxiedWebResourceSource(
        const WebPageResourceIndex* webPagesIndex,
        const QnMediaServerResourcePtr& server);

    virtual QVector<QnResourcePtr> getResources() override;

private:
    const WebPageResourceIndex* m_webPagesIndex;
    const QnMediaServerResourcePtr m_server;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
