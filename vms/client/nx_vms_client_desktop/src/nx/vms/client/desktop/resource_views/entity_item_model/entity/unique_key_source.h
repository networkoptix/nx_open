// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <core/resource/resource.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

template <class Key>
class UniqueKeySource
{
public:
    UniqueKeySource() {}

    using SetKeysHandler = std::function<void(const QVector<Key>& keyVector)>;
    using KeyNotifyHandler = std::function<void(const Key& key)>;
    using KeyNotifyHandlerPtr = std::shared_ptr<KeyNotifyHandler>;
    using InitializeRequest = std::function<void()>;

    SetKeysHandler setKeysHandler;
    KeyNotifyHandlerPtr addKeyHandler;
    KeyNotifyHandlerPtr removeKeyHandler;
    InitializeRequest initializeRequest;
};

using UniqueResourceSource = UniqueKeySource<QnResourcePtr>;
using UniqueResourceSourcePtr = std::shared_ptr<UniqueResourceSource>;

using UniqueUuidSource = UniqueKeySource<nx::Uuid>;
using UniqueUuidSourcePtr = std::shared_ptr<UniqueUuidSource>;

using UniqueStringSource = UniqueKeySource<QString>;
using UniqueStringSourcePtr = std::shared_ptr<UniqueStringSource>;

class ResourceSourceAdapter: public UniqueResourceSource
{
public:
    ResourceSourceAdapter(entity_resource_tree::AbstractResourceSourcePtr abstractResourceSource):
        m_abstractResourceSource(std::move(abstractResourceSource))
    {
        initializeRequest =
            [this]
            {
                const auto resourceSource = m_abstractResourceSource.get();

                setKeysHandler(resourceSource->getResources());

                resourceSource->connect(
                    resourceSource, &entity_resource_tree::AbstractResourceSource::resourceAdded,
                    resourceSource, *addKeyHandler);

                resourceSource->connect(
                    resourceSource, &entity_resource_tree::AbstractResourceSource::resourceRemoved,
                    resourceSource, *removeKeyHandler);
            };
    }

private:
    entity_resource_tree::AbstractResourceSourcePtr m_abstractResourceSource;
};

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
