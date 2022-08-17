// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attribute_dictionary.h"

#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>

namespace nx::utils::stree {

void copy(const AbstractIteratableContainer& from, AbstractAttributeWriter* to)
{
    for (auto it = from.begin(); !it->atEnd(); it->next())
        to->putStr(it->name(), it->value());
}

//-------------------------------------------------------------------------------------------------

AttributeDictionary::AttributeDictionary():
    base_type(&m_attrs)
{
}

AttributeDictionary::AttributeDictionary(
    std::initializer_list<std::pair<std::string_view, std::string>> l)
    :
    base_type(&m_attrs)
{
    m_attrs.reserve(l.size());
    for (const auto& [k, v]: l)
        m_attrs.emplace(k, v);
}

AttributeDictionary::AttributeDictionary(const AttributeDictionary& other):
    base_type(&m_attrs),
    m_attrs(other.m_attrs)
{
}

AttributeDictionary::AttributeDictionary(AttributeDictionary&& other):
    base_type(&m_attrs),
    m_attrs(std::move(other.m_attrs))
{
}

AttributeDictionary& AttributeDictionary::operator=(const AttributeDictionary& other)
{
    if (this != &other)
        m_attrs = other.m_attrs;

    return *this;
}

AttributeDictionary& AttributeDictionary::operator=(AttributeDictionary&& other)
{
    if (this != &other)
        m_attrs = std::move(other.m_attrs);

    return *this;
}

} // namespace nx::utils::stree
