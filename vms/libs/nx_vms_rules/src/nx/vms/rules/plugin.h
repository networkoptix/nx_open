// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaClassInfo>

#include <nx/utils/log/assert.h>

#include "engine.h"
#include "field.h"

namespace nx::vms::rules {

/**
 * Base class that provides registration mechanics for fields, events and action types.
 */
class NX_VMS_RULES_API Plugin
{
public:
    virtual ~Plugin();

    virtual void initialize(Engine* engine);
    virtual void deinitialize();

    virtual void registerFields() const;
    virtual void registerFieldValidators() const;
    virtual void registerEvents() const;
    virtual void registerActions() const;

public:
    template <class F>
    static bool registerActionField(Engine* engine, auto... args)
    {
        const QString id = fieldMetatype<F>();
        if (!NX_ASSERT(!id.isEmpty(), "Class does not have required 'metatype' property"))
            return false;

        return engine->registerActionField(id, Engine::ActionFieldRecord{
            [args...](const FieldDescriptor* descriptor){ return new F(args..., descriptor); },
            encryptedProperties<F>()});
    }

protected:
    Plugin(); // Should be used as base class only.

    template<class C>
    bool registerEventField() const
    {
        const QString id = fieldMetatype<C>();
        if (NX_ASSERT(!id.isEmpty(), "Class does not have required 'metatype' property"))
        {
            if (!m_engine->isEventFieldRegistered(id))
            {
                return m_engine->registerEventField(
                    id,
                    [](const FieldDescriptor* descriptor)
                    {
                        return new C(descriptor);
                    });
            }
        }
        return false;
    }

    template<class Field, class Validator>
    bool registerEventFieldValidator() const
    {
        return m_engine->registerEventFieldValidator(
            fieldMetatype<Field>(),
            std::make_unique<Validator>());
    }

    template<class F>
    bool registerActionField(auto... args) const
    {
        return registerActionField<F>(m_engine, args...);
    }

    template<class Field, class Validator>
    bool registerActionFieldValidator() const
    {
        return m_engine->registerActionFieldValidator(
            fieldMetatype<Field>(),
            std::make_unique<Validator>());
    }

    template<class E>
    bool registerEvent(
        const Engine::EventConstructor& constructor = []{ return new E(); }) const
    {
        const auto& manifest = E::manifest();
        return m_engine->registerEvent(manifest, constructor);
    }

    template<class E>
    bool registerEvent(
        common::SystemContext* systemContext,
        const Engine::EventConstructor& constructor = []{ return new E(); }) const
    {
        const auto& manifest = E::manifest(systemContext);
        return m_engine->registerEvent(manifest, constructor);
    }

    template<class A>
    bool registerAction(const Engine::ActionConstructor& constructor = [] { return new A(); }) const
    {
        const auto& manifest = A::manifest();
        return m_engine->registerAction(manifest, constructor);
    }

protected:
    Engine* m_engine;
};

} // namespace nx::vms::rules
