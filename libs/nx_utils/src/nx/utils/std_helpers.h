// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

namespace nx {

template<typename Container>
auto toStdVector(const Container& source)
{
    return std::vector(source.begin(), source.end());
}

} // namespace nx
