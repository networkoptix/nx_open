// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/internal_state.h>
#include <nx/utils/thread/mutex.h>

namespace nx::analytics::taxonomy {

class ObjectType;

class State: public AbstractState
{
public:
    State() = default;
    State(
        InternalState internalState,
        std::unique_ptr<AbstractResourceSupportProxy> resourceSupportProxy);

    virtual std::vector<AbstractIntegration*> integrations() const override;

    virtual std::vector<AbstractEngine*> engines() const override;

    virtual std::vector<AbstractGroup*> groups() const override;

    virtual std::vector<AbstractObjectType*> objectTypes() const override;

    virtual std::vector<AbstractEventType*> eventTypes() const override;

    virtual std::vector<AbstractEnumType*> enumTypes() const override;

    virtual std::vector<AbstractColorType*> colorTypes() const override;

    virtual std::vector<AbstractObjectType*> rootObjectTypes() const override;

    virtual std::vector<AbstractEventType*> rootEventTypes() const override;

    virtual AbstractObjectType* objectTypeById(const QString& objectTypeId) const override;

    virtual AbstractEventType* eventTypeById(const QString& eventTypeById) const override;

    virtual AbstractIntegration* integrationById(const QString& integrationId) const override;

    virtual AbstractEngine* engineById(const QString& engineId) const override;

    virtual AbstractGroup* groupById(const QString& groupId) const override;

    virtual AbstractEnumType* enumTypeById(const QString& enumTypeId) const override;

    virtual AbstractColorType* colorTypeById(const QString& colorTypeId) const override;

    virtual nx::vms::api::analytics::Descriptors serialize() const override;

    AbstractResourceSupportProxy* resourceSupportProxy() const override;

private:
    void refillCache() const;

private:
    InternalState m_internalState;

    mutable nx::Mutex m_mutex;

    mutable std::vector<AbstractIntegration*> m_cachedIntegrations;
    mutable std::vector<AbstractEngine*> m_cachedEngines;
    mutable std::vector<AbstractGroup*> m_cachedGroups;

    mutable std::vector<AbstractObjectType*> m_cachedObjectType;
    mutable std::vector<AbstractObjectType*> m_cachedRootObjectType;

    mutable std::vector<AbstractEventType*> m_cachedEventType;
    mutable std::vector<AbstractEventType*> m_cachedRootEventType;

    mutable std::vector<AbstractEnumType*> m_cachedEnumTypes;
    mutable std::vector<AbstractColorType*> m_cachedColorTypes;

    std::unique_ptr<AbstractResourceSupportProxy> m_resourceSupportProxy;
};

} // namespace nx::analytics::taxonomy
