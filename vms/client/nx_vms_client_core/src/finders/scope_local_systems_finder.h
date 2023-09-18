// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <finders/local_systems_finder.h>

/**
 * Finder creates tiles for the systems, which were added to Favorites - or hidden - but are not
 * available anymore in any way. Tiles are displayed in the corresponding mode, so user can remove
 * the system from the list.
 */
class ScopeLocalSystemsFinder: public LocalSystemsFinder
{
    Q_OBJECT
    using base_type = LocalSystemsFinder;

public:
    ScopeLocalSystemsFinder(QObject* parent = nullptr);
    virtual ~ScopeLocalSystemsFinder() = default;

private:
    virtual void updateSystems() override;
};
