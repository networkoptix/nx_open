// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_integration.h"

namespace nx::analytics::taxonomy {

QString AbstractIntegration::idAsQString() const
{
    return QString::fromStdString(id());
}

QString AbstractIntegration::nameAsQString() const
{
    return QString::fromStdString(name());
}

} // namespace nx::analytics::taxonomy
