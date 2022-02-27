// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QObject>

#include "basic_event.h"

namespace nx::vms::rules {

/**
 * Event connectors are used to transform different events that occur in the whole system
 * to a common format and than transfer them to VMS rules engine.
 */
class NX_VMS_RULES_API EventConnector: public QObject
{
    Q_OBJECT

signals:
    void ruleEvent(const EventPtr &event);
};

} // namespace nx::vms::rules
