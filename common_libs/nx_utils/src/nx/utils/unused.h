#pragma once

namespace nx {
namespace utils {

/**
 * Allows to avoid compiler warnings about unused variables.
 */
template<typename ...Ts>
void unused(Ts&&...)
{
}

} // namespace utils
} // namespace nx
