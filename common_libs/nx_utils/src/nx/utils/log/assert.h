#pragma once

#include <iostream>
#include <sstream>

#include "log_message.h"

namespace nx {
namespace utils {

template<typename Location, typename Reason>
bool assertFailure(const Location& location, const Reason& reason)
{
    #ifdef _DEBUG
        const auto message = lm(">>>FAILURE %1 %2").arg(location).arg(reason);
        std::cerr << std::endl << message.toStdString() << std::endl;

        // SIGSEGV now!
        *reinterpret_cast<volatile int*>(0) = 7;
    #else
        // NX_LOG?
    #endif

    return true;
}

} // namespace utils
} // namespace nx

#define NX_ASSERT3(expression, location, reason) \
    ((expression) || nx::utils::assertFailure(location, reason))

#define NX_ASSERT2(expression, location) \
    NX_ASSERT3(expression, location, "NX_ASSERT:" #expression " = false")

#define NX_STRING_ARG(x) #x
#define NX_ASSERT1(expression) \
    NX_ASSERT2(expression, __FILE__ ":" NX_STRING_ARG(__LINE__))

#define NX_GET_4TH_ARG(a1, a2, a3, a4, ...) a4
#define NX_ASSERT_SELECTOR(...) \
    NX_GET_4TH_ARG(__VA_ARGS__, NX_ASSERT3, NX_ASSERT2, NX_ASSERT1)

#define NX_ASSERT(...) NX_ASSERT_SELECTOR(__VA_ARGS__)(__VA_ARGS__)
