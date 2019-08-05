#pragma once

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>
#include <typeindex>

#include <nx/vms_server_plugins/analytics/stub/objects/vector2d.h>
#include <nx/vms_server_plugins/analytics/stub/objects/abstract_object.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

class AbstractObject;

float randomFloat(float min = 0, float max = 1);

Vector2D randomTrajectory();

class RandomObjectGenerator
{
public:
    using ObjectFactory = std::function<std::unique_ptr<AbstractObject>()>;

private:
    struct ObjectFactoryContext
    {
        ObjectFactoryContext(std::type_index key, ObjectFactory factory):
            key(std::move(key)),
            factory(std::move(factory))
        {
        };

        std::type_index key;
        ObjectFactory factory;
    };

public:
    template<typename ObjectType>
    void registerObjectFactory(ObjectFactory objectFactory)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = std::find_if(
            m_registry.begin(),
            m_registry.end(),
            [](const ObjectFactoryContext& value)
            {
                return std::type_index(typeid(ObjectType)) == value.key;
            });

        if (it == m_registry.cend())
            m_registry.emplace_back(typeid(ObjectType), objectFactory);
        else
            *it = ObjectFactoryContext{std::type_index(typeid(ObjectType)), objectFactory};
    }

    template<typename ObjectType>
    void unregisterObjectFactory()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = std::find_if(
            m_registry.cbegin(),
            m_registry.cend(),
            [](const ObjectFactoryContext& value)
            {
                return std::type_index(typeid(ObjectType)) == value.key;
            });

        if (it == m_registry.cend())
            return;

        m_registry.erase(it);
    }

    std::unique_ptr<AbstractObject> generate() const;

private:
    mutable std::mutex m_mutex;
    std::vector<ObjectFactoryContext> m_registry;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
