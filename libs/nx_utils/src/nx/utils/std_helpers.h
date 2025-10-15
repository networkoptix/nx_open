// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <vector>

namespace nx {

template<typename Container>
auto toStdSet(const Container& source)
{
    return std::set(source.begin(), source.end());
}

template<typename Container>
auto toStdSet(Container&& source)
{
    return std::set(
        std::make_move_iterator(source.begin()),
        std::make_move_iterator(source.end()));
}

template<typename Container>
auto toStdVector(const Container& source)
{
    return std::vector(source.begin(), source.end());
}

template<typename Container>
auto toStdVector(Container&& source)
{
    return std::vector(
        std::make_move_iterator(source.begin()),
        std::make_move_iterator(source.end()));
}

} // namespace nx
