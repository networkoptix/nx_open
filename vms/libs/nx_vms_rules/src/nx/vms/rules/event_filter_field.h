// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "field.h"

namespace nx::vms::rules {

/**
 * Abstract base class for storing configuration values for event filters.
 * Derived classes should provide Q_PROPERTY's for for all values used for
 * configuration of corresponding event field as well as match procedure
 * used to check values of incoming events in filter.
 */
class NX_VMS_RULES_API EventFilterField: public Field
{
public:
    using Field::Field;

    virtual bool match(const QVariant& value) const = 0;
};

} // namespace nx::vms::rules
