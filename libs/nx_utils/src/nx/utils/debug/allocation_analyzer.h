#pragma once

#include <memory>
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
    AllocationAnalyzer();
    virtual ~AllocationAnalyzer();

    void recordObjectCreation(void* ptr);
    void recordObjectDestruction(void* ptr);

    /**
     * @return Human-readable report which contains allocations stacks sorted by 
     * actual allocation count descending.
     */
    std::string generateReport() const;

private:
    std::unique_ptr<AllocationAnalyzerImpl> m_impl;
};

} // namespace nx::utils::debug
