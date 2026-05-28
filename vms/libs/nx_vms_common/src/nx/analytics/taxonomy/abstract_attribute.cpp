// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_attribute.h"

namespace nx::analytics::taxonomy {

QString AbstractAttribute::nameAsQString() const
{
    return QString::fromStdString(name());
}

QString AbstractAttribute::subtypeAsQString() const
{
    return QString::fromStdString(subtype());
}

QString AbstractAttribute::unitAsQString() const
{
    return QString::fromStdString(unit());
}

} // namespace nx::analytics::taxonomy
