// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

namespace nx::utils {

template<typename T>
auto toQSet(const T& collection)
{
    return QSet(collection.begin(), collection.end());
}

} // namespace nx::utils
