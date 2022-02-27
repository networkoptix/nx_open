// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

namespace std {

template<typename T>
using decay_t = typename decay<T>::type;

template<bool B, typename T, typename U>
using conditional_t = typename conditional<B,T,U>::type;

}
