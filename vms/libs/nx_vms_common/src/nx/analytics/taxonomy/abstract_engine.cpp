// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_engine.h"

namespace nx::analytics::taxonomy {

QString AbstractEngine::idAsQString() const
{
    return QString::fromStdString(id());
}

QString AbstractEngine::nameAsQString() const
{
    return QString::fromStdString(name());
}

} // namespace nx::analytics::taxonomy
