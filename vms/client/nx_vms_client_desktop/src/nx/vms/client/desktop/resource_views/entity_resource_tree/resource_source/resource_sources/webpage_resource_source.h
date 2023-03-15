// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

class QnResourcePool;

namespace nx::vms::api { enum class WebPageSubtype; }

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class WebPageResourceIndex;

class WebpageResourceSource: public AbstractResourceSource
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    using WebPageSubtype = nx::vms::api::WebPageSubtype;

    WebpageResourceSource(
        const WebPageResourceIndex* webPagesIndex,
        const std::optional<WebPageSubtype>& subtype,
        bool includeProxied);

    virtual QVector<QnResourcePtr> getResources() override;

private:
    bool isSubtypeMatched(const QnResourcePtr& resource) const;

private:
    const WebPageResourceIndex* m_webPagesIndex;
    const std::optional<WebPageSubtype> m_subtype;
    const bool m_includeProxied;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
