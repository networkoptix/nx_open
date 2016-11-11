#include "backtrace_android.h"

#include <iostream>
#include <iomanip>
#include <sstream>

#include <unwind.h>
#include <dlfcn.h>

namespace nx {
namespace utils {

namespace {

struct BacktraceState
{
    void** current;
    void** end;
};

static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg)
{
    BacktraceState* state = static_cast<BacktraceState*>(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc)
    {
        if (state->current == state->end)
            return _URC_END_OF_STACK;
        *state->current++ = reinterpret_cast<void*>(pc);
    }
    return _URC_NO_REASON;
}

} // namespace

size_t captureBacktrace(void** buffer, size_t max)
{
    BacktraceState state = {buffer, buffer + max};
    _Unwind_Backtrace(unwindCallback, &state);

    return state.current - buffer;
}

void dumpBacktrace(std::ostream& os, void** buffer, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        const void* addr = buffer[i];
        const char* symbol = "";

        Dl_info info;
        if (dladdr(addr, &info) && info.dli_sname)
            symbol = info.dli_sname;

        os << "    #" << std::setw(2) << i << ": " << addr << "  " << symbol << "\n";
    }
}

std::string buildBacktrace()
{
    const size_t kMaxSize = 30;
    void* buffer[kMaxSize];
    std::ostringstream oss;

    dumpBacktrace(oss, buffer, captureBacktrace(buffer, kMaxSize));

    return oss.str();
}

} // namespace utils
} // namespace nx
