// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "allocation_analyzer.h"

#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <sstream>

#if defined(__APPLE__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <boost/stacktrace.hpp>

#include <nx/utils/log/assert.h>

namespace nx::utils::debug {

using AllocationsByCount =
    std::multimap<int /*count*/, boost::stacktrace::stacktrace, std::greater<int>>;

class InternalAllocationAnalyzer
{
public:
    void recordObjectCreation(void* ptr, boost::stacktrace::stacktrace stack);
    bool recordObjectDestruction(void* ptr);
    AllocationsByCount getAllocationsByCount() const;

private:
    mutable std::mutex m_mutex;
    // There might be multiple ptr -> stack relations when inheritance comes into
    // play. For example with SocketDelegate used as a base class. So we need to record
    // multiple creation stacks for the same pointer and remove them several times when the
    // object is destroyed as well.
    std::map<void*, std::vector<boost::stacktrace::stacktrace>> m_ptrToStacks;
    std::map<boost::stacktrace::stacktrace, int /*count*/> m_allocationCountPerStack;
};

void InternalAllocationAnalyzer::recordObjectCreation(void* ptr, boost::stacktrace::stacktrace stack)
{
    std::lock_guard<decltype(m_mutex)> locker(m_mutex);
    m_ptrToStacks[ptr].push_back(stack);
    ++m_allocationCountPerStack[stack];
}

bool InternalAllocationAnalyzer::recordObjectDestruction(void* ptr)
{
    std::lock_guard<decltype(m_mutex)> locker(m_mutex);
    const auto it = m_ptrToStacks.find(ptr);
    NX_ASSERT(it != m_ptrToStacks.end());
    if (it == m_ptrToStacks.end())
        return false;

    const auto& stack = it->second.back();
    auto stackCountIter = m_allocationCountPerStack.find(stack);
    if (NX_ASSERT(stackCountIter != m_allocationCountPerStack.end()))
    {
        if (--stackCountIter->second == 0)
            m_allocationCountPerStack.erase(stackCountIter);
    }

    // Removing the last stack only to preserve the creation/destruction order in case of
    // multiple stacks have been recorded due to inheritance hierarchy.
    it->second.erase(--it->second.end());
    if (it->second.empty())
        m_ptrToStacks.erase(it);

    return true;
}

AllocationsByCount InternalAllocationAnalyzer::getAllocationsByCount() const
{
    std::lock_guard<decltype(m_mutex)> locker(m_mutex);
    AllocationsByCount allocations;
    for (const auto& [stack, count]: m_allocationCountPerStack)
        allocations.emplace(count, stack);

    return allocations;
}

//-------------------------------------------------------------------------------------------------

struct AllocationAnalyzerImpl
{
    InternalAllocationAnalyzer analyzer;
};

AllocationAnalyzer::AllocationAnalyzer(bool isEnabled):
    m_isEnabled(isEnabled),
    m_impl(std::make_unique<AllocationAnalyzerImpl>())
{
}

AllocationAnalyzer::~AllocationAnalyzer() = default;

void AllocationAnalyzer::recordObjectCreation([[maybe_unused]] void* ptr)
{
    if (m_isEnabled)
        m_impl->analyzer.recordObjectCreation(ptr, boost::stacktrace::stacktrace());
}

void AllocationAnalyzer::recordObjectDestruction([[maybe_unused]] void* ptr)
{
    if (m_isEnabled)
        m_impl->analyzer.recordObjectDestruction(ptr);
}

void AllocationAnalyzer::recordObjectMove([[maybe_unused]] void* ptr)
{
    if (m_isEnabled && ptr && NX_ASSERT(m_impl->analyzer.recordObjectDestruction(ptr)))
        m_impl->analyzer.recordObjectCreation(ptr, boost::stacktrace::stacktrace());
}

std::optional<std::string> AllocationAnalyzer::generateReport() const
{
    if (!m_isEnabled)
        return std::nullopt;

    const auto allocations = m_impl->analyzer.getAllocationsByCount();

    std::ostringstream report;
    report << "=====================================================================" << std::endl;
    report << "ALLOCATION REPORT BEGIN" << std::endl;
    size_t number = 1;
    for (const auto& [count, stack]: allocations)
    {
        report << std::endl
            << number << "/" << allocations.size() << ". "
            << count << " allocations done by the following stack:"<< std::endl
            << stack << std::endl;
        ++number;
    }
    report << "ALLOCATION REPORT END" << std::endl;
    report << "=====================================================================";

    return report.str();
}

} // namespace nx::utils::debug
