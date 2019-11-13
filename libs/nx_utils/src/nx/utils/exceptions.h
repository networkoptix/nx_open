#pragma once

#include <exception>
#include <string>

namespace nx::utils {

// TODO: Add macros for multiple original exceptions.

/**
 * Catches ORIGINAL_EXCEPTION thrown by EXPRESSION, wraps it and rethrows std::nested_exception
 * inherited from NEW_EXCEPTION with message MESSAGE.
 */
#define WRAP_EXCEPTION(EXPRESSION, ORIGINAL_EXCEPTION, NEW_EXCEPTION, MESSAGE) \
do { \
    try \
    { \
        EXPRESSION; \
    } catch (const ORIGINAL_EXCEPTION&)  \
    { \
        std::throw_with_nested(NEW_EXCEPTION(MESSAGE)); \
    } \
} while(false)

/**
 * Unwraps std::nested_exceptions and creates the string: "ex1.what(): ex2.what(): ..."
 */
std::string unwrapNestedErrors(const std::exception& e, std::string whats = {});

} // namespace nx::utils
