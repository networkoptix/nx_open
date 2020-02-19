#pragma once

#include <common/common_module_aware.h>

#include <core/resource_management/resource_pool.h>

#include <utils/common/connective.h>

// TODO: #ynikitenkov add versions with add/remove/specified resource slot handlers
class QnResourceChangesListener: public Connective<QObject>
{
    using base_type = Connective<QObject>;

public:
    enum class Policy
    {
        /** Call signal for all existing resources intantly after creation. */
        notifyForExisting = 0x1,

        /** Do not emit signal on resources removing. */
        silentRemove = 0x2,
    };
    Q_DECLARE_FLAGS(Policies, Policy);

    QnResourceChangesListener(QObject* parent = nullptr):
        base_type(parent)
    {
    }

    QnResourceChangesListener(Policies policies, QObject* parent = nullptr):
        base_type(parent),
        m_policies(policies)
    {
    }

    template<
        class ResourceClass,
        class ResourceSignalClass,
        class TargetClass,
        class TargetSlotClass>
    void connectToResources(
        QnResourcePool* resourcePool,
        const ResourceSignalClass& signal,
        TargetClass* receiver,
        const TargetSlotClass& slot)
    {
        connect(resourcePool, &QnResourcePool::resourceAdded, this,
            [signal, receiver, slot](const QnResourcePtr &resource)
            {
                if (const auto& target = resource.dynamicCast<ResourceClass>())
                {
                    connect(target, signal, receiver, slot);
                    (receiver->*slot)();
                }
            });

        connect(resourcePool, &QnResourcePool::resourceRemoved, this,
            [this, signal, receiver, slot](const QnResourcePtr &resource)
            {
                if (const auto& target = resource.dynamicCast<ResourceClass>())
                {
                    Qn::disconnect(target, signal, receiver, slot);
                    if (!m_policies.testFlag(Policy::silentRemove))
                        (receiver->*slot)();
                }
            });

        const auto resources = resourcePool->getResources<ResourceClass>();
        for (const auto& target: resources)
        {
            connect(target, signal, receiver, slot);
            if (m_policies.testFlag(Policy::notifyForExisting))
                (receiver->*slot)();
        }
    }

    template<class ResourceClass, class ResourceSignalClass>
    void connectToResources(
        QnResourcePool* resourcePool,
        const ResourceSignalClass& signal,
        std::function<void(const QnSharedResourcePointer<ResourceClass>& resource)> slot)
    {
        connect(resourcePool, &QnResourcePool::resourceAdded, this,
            [this, signal, slot](const QnResourcePtr& resource)
            {
                if (const auto& target = resource.dynamicCast<ResourceClass>())
                {
                    connect(target, signal, this, [target, slot]() { slot(target); });
                    slot(target);
                }
            });

        connect(resourcePool, &QnResourcePool::resourceRemoved, this,
            [this, signal, slot](const QnResourcePtr& resource)
            {
                if (const auto& target = resource.dynamicCast<ResourceClass>())
                {
                    target->disconnect(this);
                    if (!m_policies.testFlag(Policy::silentRemove))
                        slot(target);
                }
            });

        const auto resources = resourcePool->getResources<ResourceClass>();
        for (const auto& target: resources)
        {
            connect(target, signal, this, [target, slot]() { slot(target); });
            if (m_policies.testFlag(Policy::notifyForExisting))
                slot(target);
        }
    }

private:
    Policies m_policies = {};
};
