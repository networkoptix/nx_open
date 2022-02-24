// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_action.h"

#include <QMetaProperty>

namespace nx::vms::rules {

QString BasicAction::type() const
{
    //if (!m_type.isEmpty())
    //    return m_type;

    if (auto index = metaObject()->indexOfClassInfo("type"); index != -1)
        return metaObject()->classInfo(index).value();

    return {}; //< Assert?
}

} // namespace nx::vms::rules