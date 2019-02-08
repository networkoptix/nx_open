#pragma once
#if defined(ANDROID) || defined(__ANDROID__)

#include <iostream>
#include <string>

namespace nx {
namespace utils {

size_t captureBacktrace(void** buffer, size_t max);

void dumpBacktrace(std::ostream& os, void** buffer, size_t count);

/**
 * @return Possibly multi-line backtrace; each line is indented by 4 spaces and ends with "\n".
 */
std::string buildBacktrace();

} // namespace utils
} // namespace nx

#endif // defined(ANDROID) || defined(__ANDROID__)
