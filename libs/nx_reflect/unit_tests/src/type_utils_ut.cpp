// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <deque>
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <nx/reflect/instrument.h>
#include <nx/reflect/type_utils.h>

namespace nx::reflect::detail::test {

struct X {};

template<typename Expected, typename T>
void assertForward(T&& t)
{
    static_assert(std::is_same_v<Expected, decltype(detail::forward<T>(t))>, "");
}

TEST(stl_forward, implements_standard_behavior)
{
    X x;
    assertForward<X&>(x);
    assertForward<const X&>((const X&)x);
    assertForward<X&&>(std::move(x));

    const X constX;
    assertForward<const X&>(constX);
    assertForward<const X&&>(std::move(constX));
}

} // namespace nx::reflect::detail::test

//-------------------------------------------------------------------------------------------------

namespace nx::reflect::test {

static_assert(IsContainerV<std::vector<int>>, "IsContainerV, std::vector");
static_assert(IsContainerV<std::map<int, int>>, "IsContainerV, std::map");
static_assert(!IsContainerV<std::string>, "!IsContainerV, std::string");
static_assert(!IsContainerV<int>, "!IsContainerV, int");

//-------------------------------------------------------------------------------------------------

static_assert(IsAssociativeContainerV<std::map<int, int>>, "");
static_assert(IsAssociativeContainerV<std::multimap<int, int>>, "");
static_assert(IsAssociativeContainerV<std::set<int>>, "");
static_assert(IsAssociativeContainerV<std::multiset<int>>, "");

static_assert(!IsAssociativeContainerV<std::unordered_map<int, int>>, "");
static_assert(!IsAssociativeContainerV<std::unordered_multimap<int, int>>, "");
static_assert(!IsAssociativeContainerV<std::unordered_set<int>>, "");
static_assert(!IsAssociativeContainerV<std::unordered_multiset<int>>, "");

static_assert(!IsAssociativeContainerV<std::vector<int>>, "");
static_assert(!IsAssociativeContainerV<std::list<int>>, "");
static_assert(!IsAssociativeContainerV<std::deque<int>>, "");
static_assert(!IsAssociativeContainerV<std::string>, "");

//-------------------------------------------------------------------------------------------------

static_assert(!IsSetContainerV<std::map<int, int>>, "");
static_assert(!IsSetContainerV<std::multimap<int, int>>, "");
static_assert(!IsSetContainerV<std::string>, "");
static_assert(IsSetContainerV<std::set<int>>, "");
static_assert(IsSetContainerV<std::multiset<int>>, "");

//-------------------------------------------------------------------------------------------------

static_assert(!IsUnorderedSetContainerV<std::unordered_map<int, int>>, "");
static_assert(!IsUnorderedSetContainerV<std::unordered_multimap<int, int>>, "");
static_assert(!IsUnorderedSetContainerV<std::string>, "");
static_assert(IsUnorderedSetContainerV<std::unordered_set<int>>, "");
static_assert(IsUnorderedSetContainerV<std::unordered_multiset<int>>, "");

//-------------------------------------------------------------------------------------------------

static_assert(!IsUnorderedAssociativeContainerV<std::map<int, int>>, "");
static_assert(!IsUnorderedAssociativeContainerV<std::multimap<int, int>>, "");
static_assert(!IsUnorderedAssociativeContainerV<std::set<int>>, "");
static_assert(!IsUnorderedAssociativeContainerV<std::multiset<int>>, "");

static_assert(IsUnorderedAssociativeContainerV<std::unordered_map<int, int>>, "");
static_assert(IsUnorderedAssociativeContainerV<std::unordered_multimap<int, int>>, "");
static_assert(IsUnorderedAssociativeContainerV<std::unordered_set<int>>, "");
static_assert(IsUnorderedAssociativeContainerV<std::unordered_multiset<int>>, "");

//-------------------------------------------------------------------------------------------------

static_assert(!IsSequenceContainerV<std::map<int, int>>, "!IsSequenceContainerV, std::map");
static_assert(!IsSequenceContainerV<std::multimap<int, int>>, "!IsSequenceContainerV, std::multimap");
static_assert(!IsSequenceContainerV<std::unordered_map<int, int>>, "!IsSequenceContainerV, std::unordered_map");
static_assert(!IsSequenceContainerV<std::unordered_multimap<int, int>>, "!IsSequenceContainerV, std::unordered_map");
static_assert(!IsSequenceContainerV<std::set<int>>, "!IsSequenceContainerV, std::set");
static_assert(!IsSequenceContainerV<std::string>, "!IsSequenceContainerV, std::string");

static_assert(IsSequenceContainerV<std::vector<int>>, "IsSequenceContainerV, std::vector");
static_assert(IsSequenceContainerV<std::vector<std::string>>, "IsSequenceContainerV, std::vector");
static_assert(IsSequenceContainerV<std::list<int>>, "IsSequenceContainerV, std::list");
static_assert(IsSequenceContainerV<std::deque<int>>, "IsSequenceContainerV, std::deque");

} // namespace nx::reflect::test
