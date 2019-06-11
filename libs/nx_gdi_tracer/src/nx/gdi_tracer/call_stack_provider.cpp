#include "call_stack_provider.h"
#include <numeric>

namespace nx {
namespace gdi_tracer {

CallStackProvider::CallStackProvider():
    StackWalker()
{
}

std::string CallStackProvider::getCallStack(unsigned int topEntriesStripCount)
{
    ShowCallstack();
    return std::accumulate(std::next(m_callStackEntries.cbegin(), topEntriesStripCount),
        m_callStackEntries.cend(), std::string(),
        [](const std::string& a, const std::string& b)
        {
            return a + "\n" + b;
        });
}

void CallStackProvider::OnOutput(LPCSTR szText)
{
    // Empty implementation, no full debug output.
}

void CallStackProvider::OnCallstackEntry(CallstackEntryType eType, CallstackEntry& entry)
{
    if (eType == StackWalker::firstEntry)
        m_callStackEntries.clear(); //< Buffer holds single last call stack.

    m_callStackEntries.push_back(std::string(entry.moduleName)
        + " | " + std::string(entry.name)
        + " | " + std::string(entry.lineFileName)
        + " (" + std::to_string(entry.lineNumber) + ")" );
}

} // namespace gdi_tracer
} // namespace nx
