// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iterator>

namespace nx {
namespace utils {

/**
 * STL compatible output iterator which calls specified function object for each item.
 */
template<typename Item, typename Function>
class FunctionIterator
{
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using pointer = void;
    using reference = void;

    FunctionIterator(const Function& function): m_function(&function) {}
    FunctionIterator& operator=(Item item) { (*m_function)(item); return *this; }
    FunctionIterator& operator++() { return *this; }
    FunctionIterator& operator*() { return *this; }

private:
    const Function* m_function;
};

template<typename Item, typename Function>
FunctionIterator<Item, Function> functionIterator(const Function& function)
{
    return FunctionIterator<Item, Function>(function);
}

} // namespace utils
} // namespace nx
