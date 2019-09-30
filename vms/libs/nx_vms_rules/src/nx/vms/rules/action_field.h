#pragma once

#include <nx/utils/uuid.h>

#include "basic_event.h" // EventPtr
#include "field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ActionField: public Field
{
    Q_OBJECT

public:
    ActionField();

    virtual QVariant build(const EventPtr& event) const = 0;

private:

};

} // namespace nx::vms::rules
