// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string>

namespace nx::utils::debug {

struct AllocationAnalyzerImpl;

/**
 * Should be used to selectively track allocations and report current allocations.
 * This class can be useful in cases when valgrind is too slow.
 * To use it invoke AllocationAnalyzer::recordObjectCreation(this) just after creating object
 * and AllocationAnalyzer::recordObjectDestruction(this) just before destruction.
 */
class NX_UTILS_API AllocationAnalyzer
{
public:
    AllocationAnalyzer(bool isEnabled = true);
    virtual ~AllocationAnalyzer();

    void recordObjectCreation(void* ptr);
    void recordObjectDestruction(void* ptr);
    void recordObjectMove(void* ptr);

    /**
     * @return Human-readable report which contains allocations stacks sorted by
     * actual allocation count descending.
     */
    std::optional<std::string> generateReport() const;

private:
    std::atomic<bool> m_isEnabled{true};
    std::unique_ptr<AllocationAnalyzerImpl> m_impl;
};

} // namespace nx::utils::debug
