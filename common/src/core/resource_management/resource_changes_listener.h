#pragma once

#include <common/common_module_aware.h>

#include <core/resource_management/resource_pool.h>

#include <utils/common/connective.h>

// TODO: #ynikitenkov add versions with add/remove/specified resource slot handlers
class QnResourceChangesListener: public Connective<QObject>, public QnCommonModuleAware
{
    typedef Connective<QObject> base_type;
public:
    QnResourceChangesListener(QObject *parent = nullptr):
        base_type(parent),
        QnCommonModuleAware(parent)
    {}

    template<class ResourceClass, class ResourceSignalClass, class TargetClass, class TargetSlotClass>
    void connectToResources(const ResourceSignalClass &signal, TargetClass *receiver, const TargetSlotClass &slot ) {
        typedef QnSharedResourcePointer<ResourceClass> ResourceClassPtr;

        /* Call slot if resource was added or removed. */
        auto updateIfNeeded = [receiver, slot](const QnResourcePtr &resource) {
            if (resource.dynamicCast<ResourceClassPtr>())
                (receiver->*slot)();
        };

        connect(resourcePool(), &QnResourcePool::resourceAdded,   this,   updateIfNeeded);
        connect(resourcePool(), &QnResourcePool::resourceRemoved, this,   updateIfNeeded);

        connect(resourcePool(), &QnResourcePool::resourceAdded,   this,
            [signal, receiver, slot](const QnResourcePtr &resource)
            {
                if (const ResourceClassPtr &target = resource.dynamicCast<ResourceClass>())
                    connect(target, signal, receiver, slot);
            });

        connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
            [signal, receiver, slot](const QnResourcePtr &resource)
            {
                if (const ResourceClassPtr &target = resource.dynamicCast<ResourceClass>())
                    Qn::disconnect(target, signal, receiver, slot);
            });

        for (const ResourceClassPtr &target: resourcePool()->getResources<ResourceClass>())
            connect(target, signal, receiver, slot);
    }

    template<class ResourceClass, class ResourceSignalClass>
    void connectToResources(const ResourceSignalClass &signal, std::function<void()> slot ) {
        typedef QnSharedResourcePointer<ResourceClass> ResourceClassPtr;

        /* Call slot if resource was added or removed. */
        auto updateIfNeeded = [slot](const QnResourcePtr &resource) {
            if (resource.dynamicCast<ResourceClassPtr>())
                slot();
        };

        connect(resourcePool(), &QnResourcePool::resourceAdded,   this,   updateIfNeeded);
        connect(resourcePool(), &QnResourcePool::resourceRemoved, this,   updateIfNeeded);

        connect(resourcePool(), &QnResourcePool::resourceAdded,   this,   [this, signal, slot](const QnResourcePtr &resource) {
            if (const ResourceClassPtr &target = resource.dynamicCast<ResourceClass>())
                connect(target, signal, this, slot);
        });

        connect(resourcePool(), &QnResourcePool::resourceRemoved, this,   [this, signal](const QnResourcePtr &resource) {
            if (const ResourceClassPtr &target = resource.dynamicCast<ResourceClass>())
                disconnect(target, signal, this, nullptr); /* cannot directly disconnect from lambda */
        });

        for (const ResourceClassPtr &target: resourcePool()->getResources<ResourceClass>())
            connect(target, signal, this, slot);
    }


};
