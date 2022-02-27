// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

/**
 * @note Quick workaround, combining sources will be reimplemented.
 */
class NX_VMS_CLIENT_DESKTOP_API CompositeResourceSource: public AbstractResourceSource
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    CompositeResourceSource(
        AbstractResourceSourcePtr firstSource,
        AbstractResourceSourcePtr secondSource);

    virtual QVector<QnResourcePtr> getResources() override;

private:
    AbstractResourceSourcePtr m_firstSource;
    AbstractResourceSourcePtr m_secondSource;
    QSet<QnResourcePtr> m_firstSourceResources;
    QSet<QnResourcePtr> m_secondSourceResources;
};

using AbstractResourceSourcePtr = std::unique_ptr<AbstractResourceSource>;

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
