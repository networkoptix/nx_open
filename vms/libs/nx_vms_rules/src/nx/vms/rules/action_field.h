// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include "field.h"

namespace nx::vms::rules {

/**
 * Abstract base class for storing configuration values for action builders.
 * Derived classes should provide Q_PROPERTY's for for all values used for
 * configuration of corresponding action field as well as build procedure
 * used to get actual field value for given values and given event.
 */
class NX_VMS_RULES_API ActionField: public Field
{
    Q_OBJECT

public:
    ActionField();

    virtual QSet<QString> requiredEventFields() const;
    virtual QVariant build(const EventAggregatorPtr& eventAggregator) const = 0;
};

} // namespace nx::vms::rules
