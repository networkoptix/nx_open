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

namespace detail {

using AttributesMap = std::map<std::string, std::string, nx::utils::StringLessTransparentComp>;

class AttributeDictionaryConstIterator:
    public AbstractConstIterator
{
public:
    AttributeDictionaryConstIterator(const AttributesMap& attrs);

    virtual bool next() override;
    virtual bool atEnd() const override;
    virtual AttrName name() const override;
    virtual const std::string& value() const override;

private:
    const AttributesMap& m_attrs;
    typename AttributesMap::const_iterator m_curIter;
};

AttributeDictionaryConstIterator::AttributeDictionaryConstIterator(
    const AttributesMap& attrs)
    :
    m_attrs(attrs),
    m_curIter(m_attrs.begin())
{
}

bool AttributeDictionaryConstIterator::next()
{
    if (m_curIter == m_attrs.end())
        return false;
    ++m_curIter;
    return m_curIter != m_attrs.end();
}

bool AttributeDictionaryConstIterator::atEnd() const
{
    return m_curIter == m_attrs.end();
}

AttrName AttributeDictionaryConstIterator::name() const
{
    return m_curIter->first;
}

const std::string& AttributeDictionaryConstIterator::value() const
{
    return m_curIter->second;
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

AttributeDictionary::AttributeDictionary(
    std::initializer_list<std::pair<const std::string, std::string>> l)
    :
    m_attrs(std::move(l), decltype(m_attrs)::allocator_type())
{
}

std::optional<std::string> AttributeDictionary::getStr(const AttrName& name) const
{
    auto it = m_attrs.find(name);
    return it != m_attrs.end() ? std::make_optional(it->second) : std::nullopt;
}

void AttributeDictionary::putStr(const AttrName& name, std::string value)
{
    auto it = m_attrs.find(name);
    if (it == m_attrs.end())
        m_attrs[std::string(name)] = std::move(value);
    else
        it->second = std::move(value);
}

std::unique_ptr<AbstractConstIterator> AttributeDictionary::begin() const
{
    return std::make_unique<detail::AttributeDictionaryConstIterator>(m_attrs);
}

std::string AttributeDictionary::toString() const
{
    return nx::utils::join(
        m_attrs.begin(), m_attrs.end(), ", ",
        [](std::string* out, const auto& item)
        {
            return buildString(out, item.first, ':', item.second);
        });
}

bool AttributeDictionary::empty() const
{
    return m_attrs.empty();
}

void AttributeDictionary::merge(AttributeDictionary&& other)
{
    m_attrs.merge(std::move(other.m_attrs));
}

void AttributeDictionary::merge(const AttributeDictionary& other)
{
    std::copy(other.m_attrs.begin(), other.m_attrs.end(), std::inserter(m_attrs, m_attrs.end()));
}

//-------------------------------------------------------------------------------------------------
// class SingleAttributeDictionary.

SingleReader::SingleReader(const AttrName& name, std::string value):
    m_name(name),
    m_value(std::move(value))
{
}

std::optional<std::string> SingleReader::getStr(const AttrName& name) const
{
    return name == m_name ? std::make_optional(m_value) : std::nullopt;
}

} // namespace nx::utils::stree
