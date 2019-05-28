#pragma once

#include <StackWalker.h>
#include <string>
#include <vector>

namespace nx {
namespace gdi_tracer {

class CallStackProvider: public StackWalker
{
public:
    CallStackProvider();
    std::string getCallStack(unsigned int topEntriesStripCount = 0);

protected:
    virtual void OnOutput(LPCSTR szText);
    virtual void OnCallstackEntry(CallstackEntryType eType, CallstackEntry& entry);

private:
    std::vector<std::string> m_callStackEntries;
};

} // namespace gdi_tracer
} // namespace nx
