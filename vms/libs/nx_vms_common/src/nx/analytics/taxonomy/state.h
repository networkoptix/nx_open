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
    State(InternalState internalState);

    virtual std::vector<AbstractPlugin*> plugins() const override;

    virtual std::vector<AbstractEngine*> engines() const override;

    virtual std::vector<AbstractGroup*> groups() const override;

    virtual std::vector<AbstractObjectType*> objectTypes() const override;

    virtual std::vector<AbstractEnumType*> enumTypes() const override;

    virtual std::vector<AbstractColorType*> colorTypes() const override;

    virtual std::vector<AbstractObjectType*> rootObjectTypes() const override;

    virtual AbstractObjectType* objectTypeById(const QString& objectTypeId) const override;

    virtual AbstractPlugin* pluginById(const QString& pluginId) const override;

    virtual AbstractEngine* engineById(const QString& engineId) const override;

    virtual AbstractGroup* groupById(const QString& groupId) const override;

    virtual AbstractEnumType* enumTypeById(const QString& enumTypeId) const override;

    virtual AbstractColorType* colorTypeById(const QString& colorTypeId) const override;

    virtual nx::vms::api::analytics::Descriptors serialize() const override;

private:
    void refillCache() const;

private:
    InternalState m_internalState;

    mutable nx::Mutex m_mutex;

    mutable std::vector<AbstractPlugin*> m_cachedPlugins;
    mutable std::vector<AbstractEngine*> m_cachedEngines;
    mutable std::vector<AbstractGroup*> m_cachedGroups;

    mutable std::vector<AbstractObjectType*> m_cachedObjectType;
    mutable std::vector<AbstractObjectType*> m_cachedRootObjectType;

    mutable std::vector<AbstractEnumType*> m_cachedEnumTypes;
    mutable std::vector<AbstractColorType*> m_cachedColorTypes;
};

} // namespace nx::analytics::taxonomy
