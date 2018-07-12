#pragma once

#include <array>
#include <type_traits>
#include <utility>

/**@file
  * This file is for c++ utilities that are absent in modern c++ standards but can be easily
  * implemented using c++14. Some of them are in std::experimental, some present on selected
  * compilers, some are waiting in c++20 and further standards.
 */

namespace nx {
namespace utils {

/*
 * While declaring c arrays one may omit size if uses initialization, e.g.: int x[] = {9, 8, 7};
 * Unfortunately this is impossible for std::array<T, N>. "make_aray" solves this problem.
 * It returns an array of appropriate size, i.g. make_aray(9, 8, 7) return std:array<int, 3>
 * The type of element is deducted using std::common_type rules. If needed, one may infLuence the
 * deduction result simply by explicit instantiating, i.e. make_aray<std::string>("yes", "no")
 *
 * The make_array implementation in std::experimental is complicated and takes into account all
 * kinds of details concerning cv-decay, R-value references and so on. And committee is still
 * improving it. Here is much more simple implementation that is enough for most cases.
 */
template<typename... Ts>
constexpr std::array<std::common_type_t<Ts...>, sizeof...(Ts)> make_array(Ts&&... ts)
{
    return {std::forward<Ts>(ts)...};
}

}   //namespace utils
}   //namespace nx
