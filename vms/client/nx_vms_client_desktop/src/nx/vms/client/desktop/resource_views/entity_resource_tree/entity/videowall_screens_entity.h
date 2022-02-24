// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_list_entity.h>
#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using VideoWallScreenItemCreator =
    std::function<entity_item_model::AbstractItemPtr(const QnUuid&)>;

/**
 * Entity which represents list of Video Wall Screens taken directly from given resource.
 */
class VideoWallScreensEntity: public entity_item_model::UniqueKeyListEntity<QnUuid>
{
    using base_type = entity_item_model::UniqueKeyListEntity<QnUuid>;

public:
    VideoWallScreensEntity(
        const VideoWallScreenItemCreator& itemCreator,
        const QnVideoWallResourcePtr& videoWall);

private:
    nx::utils::ScopedConnections m_connectionsGuard;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
