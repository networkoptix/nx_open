#pragma once

#include <exception>
#include <string>

namespace nx::utils {

// TODO: Add macros for multiple original exceptions.

/**
 * Catches ORIGINAL_EXCEPTION thrown by EXPRESSION, wraps it and rethrows std::nested_exception
 * inherited from NEW_EXCEPTION with message MESSAGE.
 */
#define NX_WRAP_EXCEPTION(EXPRESSION, ORIGINAL_EXCEPTION, NEW_EXCEPTION, MESSAGE) [&]() \
{ \
    try \
    { \
        return EXPRESSION; \
    } catch (const ORIGINAL_EXCEPTION&)  \
    { \
        std::throw_with_nested(NEW_EXCEPTION(MESSAGE)); \
    } \
}()

/**
 * Unwraps std::nested_exceptions and creates the string: "ex1.what(): ex2.what(): ..."
 */
std::string NX_UTILS_API unwrapNestedErrors(const std::exception& e, std::string whats = {});

} // namespace nx::utils
