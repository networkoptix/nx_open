// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "nx/analytics/taxonomy/abstract_engine.h"
#include "nx/analytics/taxonomy/abstract_group.h"

namespace nx::analytics::taxonomy {

class Engine;
class Group;

class NX_VMS_COMMON_API Scope
{
public:
    AbstractEngine* engine() const;

    AbstractGroup* group() const;

    QString provider() const;

    bool hasTypeEverBeenSupportedInThisScope() const;

    bool isEmpty() const;


    void setEngine(Engine* engine);

    void setGroup(Group* group);

    void setProvider(const QString& provider);

    void setHasTypeEverBeenSupportedInThisScope(bool hasTypeEverBeenSupportedInThisScope);

private:
    Engine* m_engine = nullptr;
    Group* m_group = nullptr;
    QString m_provider;
    bool m_hasTypeEverBeenSupportedInThisScope = false;
};

} // namespace nx::analytics::taxonomy
