// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

namespace nx::vms::rules {

Plugin::Plugin()
{
}

Plugin::~Plugin()
{
}

void Plugin::initialize(Engine* engine)
{
    m_engine = engine;

    registerFields();
    registerEvents();
    registerActions();
}

void Plugin::registerFields() const
{
}

void Plugin::registerEvents() const
{
}

void Plugin::registerActions() const
{
}

void Plugin::deinitialize()
{
}

} // namespace nx::vms::rules
