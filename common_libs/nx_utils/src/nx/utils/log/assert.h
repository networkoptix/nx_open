#pragma once

#include <iostream>
#include <sstream>

#include "log_message.h"


namespace nx {
namespace utils {

template<typename Reason>
bool assertFailure(
    const char* file, int line, const char* condition, const Reason& message)
{
    const auto out = lm(">>>FAILURE %1:%2 NX_ASSERT(%3) %4")
        .arg(file).arg(line).arg(condition).arg(message);

    std::cerr << std::endl << out.toStdString() << std::endl;

    // SIGSEGV now!
    *reinterpret_cast<volatile int*>(0) = 7;
    return true;
}

} // namespace utils
} // namespace nx


#define NX_CRITICAL_IMPL(condition, message) \
    static_cast<void>((condition) || \
        nx::utils::assertFailure(__FILE__, __LINE__, #condition, message))

#ifdef _DEBUG
    #define NX_ASSERT_IMPL(condition, message) NX_CRITICAL_IMPL(condition, message)
#else
    #define NX_ASSERT_IMPL(condition, message)
#endif


#define NX_MSVC_EXPAND(x) x
#define NX_GET_4TH_ARG(a1, a2, a3, a4, ...) a4


#define NX_CRITICAL1(condition) \
    NX_CRITICAL_IMPL(condition, "")

#define NX_CRITICAL2(condition, message) \
    NX_CRITICAL_IMPL(condition, message)

#define NX_CRITICAL3(condition, where, message) \
    NX_CRITICAL_IMPL(condition, lm("[%1] %2").arg(where).arg(message))

#define NX_CRITICAL(...) NX_MSVC_EXPAND( \
    NX_GET_4TH_ARG(__VA_ARGS__, NX_CRITICAL3, NX_CRITICAL2, NX_CRITICAL1)(__VA_ARGS__))


#define NX_ASSERT1(condition) \
    NX_ASSERT_IMPL(condition, "")

#define NX_ASSERT2(condition, message) \
    NX_ASSERT_IMPL(condition, message)

#define NX_ASSERT3(condition, where, message) \
    NX_ASSERT_IMPL(condition, lm("[%1] %2").arg(where).arg(message))

#define NX_ASSERT(...) NX_MSVC_EXPAND( \
    NX_GET_4TH_ARG(__VA_ARGS__, NX_ASSERT3, NX_ASSERT2, NX_ASSERT1)(__VA_ARGS__))
