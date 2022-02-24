// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <nx/utils/type_traits.h>

namespace nx::utils {

struct Foo {};

struct FooIterable
{
    using iterator = typename std::vector<int>::iterator;
    using const_iterator = typename std::vector<int>::const_iterator;

    const_iterator begin() const { return const_iterator(); }
    const_iterator end() const { return const_iterator(); }

    iterator begin() { return iterator(); }
    iterator end() { return iterator(); }
};

TEST(IsIterable, works)
{
    static_assert(IsIterableV<std::vector<int>>);
    static_assert(IsIterableV<FooIterable>);
    static_assert(IsIterableV<std::string>);
    static_assert(IsIterableV<std::vector<Foo>>);

    static_assert(!IsIterableV<Foo>);
    static_assert(!IsIterableV<int>);
    static_assert(!IsIterableV<const Foo*>);
    static_assert(!IsIterableV<int*>);
    static_assert(!IsIterableV<typename std::vector<int>::iterator>);
}

TEST(IsIterator, works)
{
    static_assert(IsIteratorV<typename std::vector<int>::iterator>);
    static_assert(IsIteratorV<const Foo*>);
    static_assert(IsIteratorV<int*>);

    static_assert(!IsIteratorV<std::string>);
    static_assert(!IsIteratorV<std::vector<Foo>>);
    static_assert(!IsIteratorV<Foo>);
    static_assert(!IsIteratorV<FooIterable>);
    static_assert(!IsIteratorV<int>);
}

} // namespace nx::utils
