// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QMetaClassInfo>

#include <nx/utils/uuid.h>
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
    void registerEventField() const
    {
        int idx = C::staticMetaObject.indexOfClassInfo("metatype");
        NX_ASSERT(idx >= 0, "Class does not have required 'metatype' property");

        m_engine->registerEventField(C::staticMetaObject.classInfo(idx).value(), []{ return new C(); });
    }

    template<class C>
    void registerActionField() const
    {
        int idx = C::staticMetaObject.indexOfClassInfo("metatype");
        NX_ASSERT(idx >= 0, "Class does not have required 'metatype' property");

        m_engine->registerActionField(C::staticMetaObject.classInfo(idx).value(), []{ return new C(); });
    }

private:
    Engine* m_engine;
};

} // namespace nx::vms::rules
