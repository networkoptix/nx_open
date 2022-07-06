// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <nx/reflect/string_conversion.h>
#include <nx/utils/uuid.h>
#include <nx/utils/string.h>

namespace nx::utils::stree {

using AttrName = std::string_view;

/**
 * Implement this interface to allow reading attribute values from object.
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
            if constexpr (std::is_integral_v<T>)
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

    template<>
    std::optional<std::string> get<std::string>(const AttrName& name) const
    {
        return getStr(name);
    }

    template<>
    std::optional<bool> get<bool>(const AttrName& name) const
    {
        if (auto str = getStr(name); str)
            return std::make_optional(nx::utils::stricmp(*str, "true") == 0 || *str == "1");

        return std::nullopt;
    }

    template<>
    std::optional<QnUuid> get<QnUuid>(const AttrName& name) const
    {
        if (auto str = getStr(name); str)
        {
            QnUuid val = QnUuid::fromStringSafe(*str);
            if (!val.isNull())
                return val;
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
 * Implement this interface to allow writing attribute values to object.
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
 * Implement this interface to allow walk through container data.
 * Usage sample:
 * AbstractIteratableContainer* cont = ...;
 * for( auto it = cont->begin(); !it->atEnd(); it->next() )
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

/**
 * Allows to add/get attributes. Represents associative container.
 */
class NX_UTILS_API AttributeDictionary:
    public AbstractAttributeReader,
    public AbstractAttributeWriter,
    public AbstractIteratableContainer
{
public:
    AttributeDictionary() = default;
    AttributeDictionary(std::initializer_list<std::pair<const std::string, std::string>> l);

    virtual std::optional<std::string> getStr(const AttrName& name) const override;
    virtual void putStr(const AttrName& name, std::string value) override;

    virtual std::unique_ptr<nx::utils::stree::AbstractConstIterator> begin() const override;

    std::string toString() const;
    bool empty() const;

private:
    std::map<std::string, std::string, nx::utils::StringLessTransparentComp> m_attrs;
};

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API SingleReader:
    public AbstractAttributeReader
{
public:
    SingleReader(const AttrName& name, std::string value);

    virtual std::optional<std::string> getStr(const AttrName& name) const override;

private:
    const std::string m_name;
    std::string m_value;
};

//-------------------------------------------------------------------------------------------------

/**
 * Reads from mutiple AbstractAttributeReader.
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
