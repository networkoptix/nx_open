// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <nx/reflect/string_conversion.h>
#include <nx/utils/string.h>
#include <nx/utils/uuid.h>

namespace nx::utils::stree {

using AttrName = std::string_view;

/**
 * Implement this interface to allow reading attributes.
 */
class NX_UTILS_API AbstractAttributeReader
{
public:
    virtual ~AbstractAttributeReader() = default;

    /**
     * @return Value of attribute with the given name if found. std::nullopt otherwise.
     */
    virtual std::optional<std::string> getStr(const AttrName& name) const = 0;

    /**
     * An implementation may want to override this method to add support for non-unique attributes.
     * @return All values of attribute with the given name.
     */
    virtual std::vector<std::string> getStrAll(const AttrName& name) const
    {
        if (auto value = getStr(name); value)
            return {std::move(*value)};
        return {};
    }

    virtual bool contains(const AttrName& name) const
    {
        return getStr(name) != std::nullopt;
    }

    /**
     * Helper function to get typed value.
     * @return Value if attribute was found and converted to T, std::nullopt otherwise.
     */
    template<typename T>
    std::optional<T> get(const AttrName& name) const
    {
        if (auto str = getStr(name); str)
        {
            if constexpr (std::is_same_v<T, bool>)
            {
                return std::make_optional(nx::utils::stricmp(*str, "true") == 0 || *str == "1");
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                return str;
            }
            else if constexpr (std::is_same_v<T, nx::Uuid>)
            {
                nx::Uuid val = nx::Uuid::fromStringSafe(*str);
                if (!val.isNull())
                    return val;
            }
            else if constexpr (std::is_integral_v<T>)
            {
                std::size_t parsedChars = 0;
                const T val = nx::utils::ston<T>(*str, &parsedChars, /*base*/ 10);
                return parsedChars > 0 ? std::make_optional(val) : std::nullopt;
            }
            else
            {
                bool ok = false;
                auto value = nx::reflect::fromString<T>(str, &ok);
                return ok ? std::make_optional(std::move(value)) : std::nullopt;
            }
        }

        return std::nullopt;
    }

    template<typename T>
    T getOr(const AttrName& name, T default_) const
    {
        auto value = get<T>(name);
        return value ? *value : default_;
    }
};

//-------------------------------------------------------------------------------------------------

/**
 * Implement this interface to allow adding attributes.
 */
class NX_UTILS_API AbstractAttributeWriter
{
public:
    virtual ~AbstractAttributeWriter() = default;

    /**
     * If attribute with name already exists, its value is overwritten with value.
     * Otherwise, new attribute added.
     */
    virtual void putStr(const AttrName& name, std::string value) = 0;

    template<typename T>
    void put(const AttrName& name, const T& value)
    {
        putStr(name, nx::reflect::toString(value));
    }
};

//-------------------------------------------------------------------------------------------------

/**
 * Iterating over attributes.
 */
class NX_UTILS_API AbstractConstIterator
{
public:
    virtual ~AbstractConstIterator() = default;

    /**
     * @return false if already at the end.
     */
    virtual bool next() = 0;

    virtual bool atEnd() const = 0;
    virtual AttrName name() const = 0;
    virtual const std::string& value() const = 0;

    template<typename T>
    T value(bool* ok = nullptr) const
    {
        T val;
        bool parsed = nx::reflect::fromString(value(), &val);
        if (ok)
            *ok = parsed;
        return val;
    }
};

//-------------------------------------------------------------------------------------------------

/**
 * Interface of an attributes dictionary that allows iterating.
 * Usage sample:
 * AbstractIteratableContainer* cont = ...;
 * for (auto it = cont->begin(); !it->atEnd(); it->next())
 * {
 *   //accessing data through iterator
 * }
 */
class NX_UTILS_API AbstractIteratableContainer
{
public:
    virtual ~AbstractIteratableContainer() = default;

    /**
     * @return Iterator set to the first element.
     */
    virtual std::unique_ptr<nx::utils::stree::AbstractConstIterator> begin() const = 0;
};

NX_UTILS_API void copy(const AbstractIteratableContainer& from, AbstractAttributeWriter* to);

//-------------------------------------------------------------------------------------------------

namespace detail {

template<typename AttributesMap>
class AttributeDictionaryConstIterator:
    public AbstractConstIterator
{
public:
    AttributeDictionaryConstIterator(const AttributesMap& attrs):
        m_attrs(attrs),
        m_curIter(m_attrs.begin())
    {
    }

    virtual bool next() override
    {
        if (m_curIter == m_attrs.end())
            return false;
        ++m_curIter;
        return m_curIter != m_attrs.end();
    }

    virtual bool atEnd() const override { return m_curIter == m_attrs.end(); }
    virtual AttrName name() const override { return m_curIter->first; }
    virtual const std::string& value() const override { return m_curIter->second; }

private:
    const AttributesMap& m_attrs;
    typename AttributesMap::const_iterator m_curIter;
};

template<typename C, typename Key, typename = void>
struct FindDefined: public std::false_type {};

template<typename C, typename Key>
struct FindDefined<C, Key, std::void_t<decltype(C().find(Key()))>>:
    public std::true_type {};

template<typename... Args> inline constexpr bool FindDefinedV = FindDefined<Args...>::value;

} // namespace detail

//-------------------------------------------------------------------------------------------------

/**
 * Adapter for using an STL associative container as AbstractAttributeReader.
 */
template<typename AttributesMap>
class AttributeReaderAdapter:
    public AbstractAttributeReader
{
public:
    AttributeReaderAdapter(const AttributesMap& attrs): m_attrs(&attrs) {}
    AttributeReaderAdapter(const AttributesMap* attrs): m_attrs(attrs) {}

    virtual std::optional<std::string> getStr(const AttrName& name) const override
    {
        typename AttributesMap::const_iterator it;
        if constexpr (detail::FindDefinedV<AttributesMap, AttrName>)
            it = m_attrs->find(name);
        else
            it = m_attrs->find(std::string(name));

        return it != m_attrs->end() ? std::make_optional(it->second) : std::nullopt;
    }

private:
    const AttributesMap* m_attrs = nullptr;
};

//-------------------------------------------------------------------------------------------------

/**
 * Adapts an STL associative container as an attributes container.
 */
template<typename AttributesMap>
class AttributeDictionaryAdapter:
    public AttributeReaderAdapter<AttributesMap>,
    public AbstractAttributeWriter,
    public AbstractIteratableContainer
{
public:
    AttributeDictionaryAdapter(AttributesMap* attrs):
        AttributeReaderAdapter<AttributesMap>(attrs),
        m_attrs(attrs)
    {
    }

    virtual void putStr(const AttrName& name, std::string value) override
    {
        typename AttributesMap::iterator it;
        if constexpr (detail::FindDefinedV<AttributesMap, AttrName>)
            it = m_attrs->find(name);
        else
            it = m_attrs->find(std::string(name));

        if (it == m_attrs->end())
            (*m_attrs)[std::string(name)] = std::move(value);
        else
            it->second = std::move(value);
    }

    virtual std::unique_ptr<nx::utils::stree::AbstractConstIterator> begin() const override
    {
        return std::make_unique<detail::AttributeDictionaryConstIterator<AttributesMap>>(*m_attrs);
    }

    std::string toString() const
    {
        return nx::utils::join(
            m_attrs->begin(), m_attrs->end(), ", ",
            [](std::string* out, const auto& item)
            {
                return buildString(out, item.first, ':', item.second);
            });
    }

    bool empty() const
    {
        return m_attrs->empty();
    }

    /**
     * Inserts elements of other with keys missing in *this.
     */
    void merge(AttributeDictionaryAdapter&& other)
    {
        m_attrs->merge(std::move(*other.m_attrs));
    }

    void merge(const AttributeDictionaryAdapter& other)
    {
        std::copy(other.m_attrs->begin(), other.m_attrs->end(), std::inserter(*m_attrs, m_attrs->end()));
    }

private:
    AttributesMap* m_attrs = nullptr;
};

//-------------------------------------------------------------------------------------------------

using StringAttrDict = std::unordered_map<std::string, std::string,
    nx::utils::StringHashTransparent, nx::utils::StringEqualToTransparent>;

/**
 * Attributes container. Implements reading and saving attributes.
 * Attributes are stored in a unique-key dictionary internally.
 */
class NX_UTILS_API AttributeDictionary:
    public AttributeDictionaryAdapter<StringAttrDict>
{
public:
    using Container = StringAttrDict;

private:
    using base_type = AttributeDictionaryAdapter<Container>;

public:
    AttributeDictionary();
    AttributeDictionary(std::initializer_list<std::pair<std::string_view, std::string>> l);

    AttributeDictionary(Container attrs):
        base_type(&m_attrs),
        m_attrs(std::move(attrs))
    {
    }

    template<typename Iter>
    AttributeDictionary(Iter begin, Iter end):
        base_type(&m_attrs),
        m_attrs(begin, end)
    {
    }

    AttributeDictionary(const AttributeDictionary&);
    AttributeDictionary(AttributeDictionary&&);

    AttributeDictionary& operator=(const AttributeDictionary&);
    AttributeDictionary& operator=(AttributeDictionary&&);

    const Container& data() const { return m_attrs; }
    Container takeData() { return std::exchange(m_attrs, {}); }

private:
    Container m_attrs;
};

//-------------------------------------------------------------------------------------------------

/**
 * Reads from mutiple AbstractAttributeReader instances.
 */
template<std::size_t kSize>
class MultiReader:
    public AbstractAttributeReader
{
public:
    MultiReader(
        std::array<std::reference_wrapper<const AbstractAttributeReader>, kSize> readers)
        :
        m_readers(std::move(readers))
    {
    }

    /**
     * Queries all readers and returns first value found.
     */
    virtual std::optional<std::string> getStr(const AttrName& name) const override
    {
        for (const auto reader: m_readers)
        {
            if (auto value = reader.get().getStr(name); value)
                return value;
        }

        return std::nullopt;
    }

    virtual std::vector<std::string> getStrAll(const AttrName& name) const override
    {
        std::vector<std::string> values;
        for (const auto reader: m_readers)
        {
            auto vals = reader.get().getStrAll(name);
            std::move(vals.begin(), vals.end(), std::back_inserter(values));
        }

        return values;
    }

private:
    std::array<std::reference_wrapper<const AbstractAttributeReader>, kSize> m_readers;
};

template<typename... Readers>
MultiReader<sizeof...(Readers)> makeMultiReader(const Readers&... readers)
{
    return MultiReader<sizeof...(Readers)>({readers...});
}

//-------------------------------------------------------------------------------------------------

/**
 * Writes to all writers.
 */
template<std::size_t kSize>
class MultiWriter:
    public AbstractAttributeWriter
{
public:
    MultiWriter(std::array<AbstractAttributeWriter*, kSize> writers):
        m_writers(std::move(writers))
    {
    }

    virtual void putStr(const AttrName& name, std::string value) override
    {
        for (auto& writer: m_writers)
            writer->putStr(name, value);
    }

private:
    std::array<AbstractAttributeWriter*, kSize> m_writers;
};

template<typename... Writers>
MultiWriter<sizeof...(Writers)> makeMultiWriter(const Writers&... writers)
{
    return MultiWriter<sizeof...(Writers)>({writers...});
}

} // namespace nx::utils::stree
