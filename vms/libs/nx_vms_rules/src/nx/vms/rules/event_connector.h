// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include "rules_fwd.h"

namespace nx::vms::rules {

/**
 * Event connectors are used to transform different events that occur in the whole system
 * to a common format and than transfer them to VMS rules engine.
 */
class NX_VMS_RULES_API EventConnector: public QObject
{
    Q_OBJECT

signals:
    void event(const nx::vms::rules::EventPtr &event);

    // TODO: #amalov Think of more general interface.
    void analyticsEvents(const std::vector<nx::vms::rules::EventPtr>& events);
};

} // namespace nx::vms::rules
