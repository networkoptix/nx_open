#pragma once

#include <nx/utils/uuid.h>

namespace nx::vms::rules {

class NX_VMS_RULES_API EventField: public QObject
{
    Q_OBJECT

public:
    EventField();

    virtual bool match(const QVariant& value) const = 0;
};

} // namespace nx::vms::rules
