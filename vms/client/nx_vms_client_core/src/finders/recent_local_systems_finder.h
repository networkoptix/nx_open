// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <finders/local_systems_finder.h>

/**
 * Handles recent connections, stored in the coreSettings()->recentLocalConnections property.
 */
class QnRecentLocalSystemsFinder: public LocalSystemsFinder
{
    Q_OBJECT
    using base_type = LocalSystemsFinder;

public:
    QnRecentLocalSystemsFinder(QObject* parent = nullptr);
    virtual ~QnRecentLocalSystemsFinder() = default;

private:
    virtual void updateSystems();

};
