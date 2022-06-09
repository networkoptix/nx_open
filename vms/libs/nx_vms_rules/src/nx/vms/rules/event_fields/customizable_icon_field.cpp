// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "customizable_icon_field.h"

#include <QtCore/QVariant>

#include <nx/utils/log/assert.h>

namespace nx::vms::rules {

CustomizableIconField::CustomizableIconField()
{
}

bool CustomizableIconField::match(const QVariant& value) const
{
    // Field value used for event customization, matches any event value.
    return true;
}

QString CustomizableIconField::name() const
{
    return m_name;
}

void CustomizableIconField::setName(const QString& name)
{
    m_name = name;
}

} // namespace nx::vms::rules
