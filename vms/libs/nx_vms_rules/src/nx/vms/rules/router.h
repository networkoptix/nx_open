// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QObject>

#include <nx/utils/uuid.h>

#include "rules_fwd.h"

namespace nx::vms::rules {

/**
 * Abstract class that provides interface for event router.
 * This router exists in a single copy as a part of every Engine instance
 * and is responsible for event delivery between different Engine instances,
 * however it was split out into a separate class for testing purposes.
 */
class NX_VMS_RULES_API Router: public QObject
{
    Q_OBJECT

public:
    Router();
    virtual ~Router();

    virtual void init(const nx::Uuid& id);

    virtual void routeEvent(
        const EventData& eventData,
        const QSet<nx::Uuid>& triggeredRules, //< TODO: #spanasenko Use filter IDs instead.
        const QSet<nx::Uuid>& affectedResources) = 0;

signals:
    void eventReceived(const nx::Uuid& ruleId, const EventData& eventData);
};

} // namespace nx::vms::rules
