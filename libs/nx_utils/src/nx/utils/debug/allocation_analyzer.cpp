#include "allocation_analyzer.h"

#include <chrono>
#include <map>
#include <mutex>
#include <sstream>

#if defined(__APPLE__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <boost/stacktrace.hpp>

namespace nx::utils::debug {

using AllocationsByCount =
    std::map<int /*count*/, boost::stacktrace::stacktrace, std::greater<int>>;

class InternalAllocationAnalyzer
{
public:
    void recordObjectCreation(void* ptr, boost::stacktrace::stacktrace stack);
    void recordObjectDestruction(void* ptr);
    AllocationsByCount getAllocationsByCount() const;

private:
    mutable std::mutex m_mutex;
    std::map<void*, boost::stacktrace::stacktrace> m_ptrToStack;
    std::map<boost::stacktrace::stacktrace, int /*count*/> m_allocationCountPerStack;
};

void InternalAllocationAnalyzer::recordObjectCreation(void* ptr, boost::stacktrace::stacktrace stack)
{
    std::lock_guard<decltype(m_mutex)> locker(m_mutex);

    m_ptrToStack[ptr] = stack;
    ++m_allocationCountPerStack[stack];
}

void InternalAllocationAnalyzer::recordObjectDestruction(void* ptr)
{
    std::lock_guard<decltype(m_mutex)> locker(m_mutex);

    auto it = m_ptrToStack.find(ptr);
    if (it == m_ptrToStack.end())
        return;
    
    auto stackCountIter = m_allocationCountPerStack.find(it->second);
    if (stackCountIter != m_allocationCountPerStack.end())
    {
        --stackCountIter->second;
        if (stackCountIter->second == 0)
            m_allocationCountPerStack.erase(stackCountIter);
    }

    m_ptrToStack.erase(it);
}

AllocationsByCount InternalAllocationAnalyzer::getAllocationsByCount() const
{
    std::lock_guard<decltype(m_mutex)> locker(m_mutex);

    AllocationsByCount allocations;
    for (auto& [stack, count]: m_allocationCountPerStack)
        allocations[count] = stack;

    return allocations;
}

//-------------------------------------------------------------------------------------------------

struct AllocationAnalyzerImpl
{
    InternalAllocationAnalyzer analyzer;
};

AllocationAnalyzer::AllocationAnalyzer():
    m_impl(std::make_unique<AllocationAnalyzerImpl>())
{
}

AllocationAnalyzer::~AllocationAnalyzer() = default;

void AllocationAnalyzer::recordObjectCreation([[maybe_unused]] void* ptr)
{
    m_impl->analyzer.recordObjectCreation(ptr, boost::stacktrace::stacktrace());
}

void AllocationAnalyzer::recordObjectDestruction([[maybe_unused]] void* ptr)
{
    m_impl->analyzer.recordObjectDestruction(ptr);
}

std::string AllocationAnalyzer::generateReport() const
{
    auto allocations = m_impl->analyzer.getAllocationsByCount();

    std::ostringstream report;
    report << "=====================================================================" << std::endl;
    report << "ALLOCATION REPORT BEGIN" << std::endl;
    for (auto& [count, stack]: allocations)
    {
        report << count << " allocations done by the following stack:"<< std::endl
            << stack << std::endl
            << std::endl
            << std::endl;
    }
    report << "ALLOCATION REPORT END" << std::endl;
    report << "=====================================================================" << std::endl;

    return report.str();
}

} // namespace nx::utils::debug
