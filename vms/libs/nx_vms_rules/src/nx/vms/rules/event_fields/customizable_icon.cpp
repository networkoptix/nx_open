// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "customizable_icon.h"

#include <QtCore/QVariant>

namespace nx::vms::rules {

CustomizableIcon::CustomizableIcon()
{
}

bool CustomizableIcon::match(const QVariant& value) const
{
    return true;
}

QString CustomizableIcon::name() const
{
    return m_name;
}

void CustomizableIcon::setName(const QString& name)
{
    m_name = name;
}

} // namespace nx::vms::rules
