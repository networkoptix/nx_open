// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaClassInfo>

#include <nx/utils/log/assert.h>

#include "engine.h"

namespace nx::vms::rules {

/**
 * Base class that provides registration mechanics for fields, events and action types.
 */
class NX_VMS_RULES_API Plugin
{
public:
    Plugin(Engine* engine);
    virtual ~Plugin();

    virtual void initialize();
    virtual void registerEventConnectors() const;
    virtual void registerFields() const;
    virtual void registerEvents() const;
    virtual void registerActions() const;
    virtual void deinitialize();

protected:
    template<class C>
    bool registerEventField() const
    {
        const QString id = fieldMetatype<C>();
        if (NX_ASSERT(!id.isEmpty(), "Class does not have required 'metatype' property"))
        {
            return m_engine->registerEventField(id, [] { return new C(); });
        }
        return false;
    }

    template<class C>
    bool registerActionField() const
    {
        const QString id = fieldMetatype<C>();
        if (NX_ASSERT(!id.isEmpty(), "Class does not have required 'metatype' property"))
        {
            return m_engine->registerActionField(id, [] { return new C(); });
        }
        return false;
    }

    template<class E>
    bool registerEvent() const
    {
        const auto& manifest = E::manifest();
        return m_engine->registerEvent(manifest);
    }

    template<class A>
    bool registerAction() const
    {
        const auto& manifest = A::manifest();

        // TODO: Decide on simultanious manifest and constructor registration.
        // Action instance may be created by executor.
        return m_engine->registerAction(manifest, [] { return new A(); });
    }

protected:
    Engine* m_engine;
};

} // namespace nx::vms::rules
