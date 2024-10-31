// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>

#include <stddef.h>
#include <stdint.h>

#include <nx/reflect/basic_serializer.h>
#include <nx/reflect/generic_visitor.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/type_utils.h>
#include <nx/reflect/utils.h>

namespace nx::reflect {

namespace detail {

constexpr const char* getSerializedTypeName(int64_t)
{
    return "int64";
}

constexpr const char* getSerializedTypeName(double)
{
    return "double";
}

template<typename... Args>
constexpr const char* getSerializedTypeName(std::chrono::duration<Args...>)
{
    return "int64";
}

template<typename... Args>
constexpr const char* getSerializedTypeName(std::chrono::time_point<Args...>)
{
    return "int64";
}

template<typename Sink, typename T>
constexpr Sink typeName(const Sink& sink);

template<typename Sink>
class FieldEnumerator
{
public:
    explicit constexpr FieldEnumerator(Sink sink): m_sink(std::move(sink)) {}

    template<typename WrappedField, typename... Fields>
    constexpr Sink operator()(const WrappedField& field, Fields... fields) const
    {
        return visitField(m_sink, field, fields...);
    }

    template<typename WrappedField>
    constexpr Sink visitField(const Sink& sink, const WrappedField& field) const
    {
        return typeName<Sink, typename WrappedField::Type>(sink.add(field.name()).add(":"))
            .add(",");
    }

    template<typename WrappedField, typename... Fields>
    constexpr Sink visitField(const Sink& sink, const WrappedField& field, Fields... fields) const
    {
        return visitField(visitField(sink, field), fields...);
    }

private:
    const Sink m_sink;
};

template<typename Sink, typename T>
constexpr Sink typeName(const Sink& sink)
{
    if constexpr (IsInstrumentedV<T>)
    {
        FieldEnumerator<Sink> fieldEnumerator(sink.add("{"));
        return nx::reflect::visitAllFields<T>(fieldEnumerator).add("}");
    }
    else if constexpr (IsStringAlikeV<T> || IsInstrumentedEnumV<T>)
    {
        return sink.add("string");
    }

    else if constexpr (!IsSetContainerV<T>
        && !IsUnorderedSetContainerV<T>
        && (IsAssociativeContainerV<T>
            || IsUnorderedAssociativeContainerV<T>))
    {
        return typeName<Sink, typename T::mapped_type>(
            typeName<Sink, typename T::key_type>(sink.add("[")).add(",")).add("]");
    }
    else if constexpr (IsContainerV<T> || IsSetContainerV<T> || IsUnorderedSetContainerV<T>)
    {
        return typeName<Sink, typename T::value_type>(sink.add("[")).add("]");
    }
    else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>)
    {
        return sink.add(getSerializedTypeName(int64_t()));
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        return sink.add(getSerializedTypeName(double()));
    }
    else
    {
        return sink.add(getSerializedTypeName(T()));
    }
}

template<typename T, T polynomial, T xorValue, T initial>
class Crc
{
public:
    constexpr Crc(): m_value(initial) {}
    constexpr explicit Crc(T value): m_value(value) {}

    constexpr Crc<T, polynomial, xorValue, initial> add(const char* str) const
    {
        T val = m_value;
        for (; str && *str; ++str)
        {
            const int bitness = sizeof(T) * 8;
            const T shiftedByte = static_cast<T>(*str) << (bitness - 8);
            val ^= shiftedByte;
            for (int bit = 0; bit < 8; ++bit)
            {
                const T msb = val & (static_cast<T>(1) << (bitness - 1));
                val = static_cast<T>(val << 1);
                if (msb != 0)
                    val ^= polynomial;
            }
        }
        return Crc<T, polynomial, xorValue, initial>(val);
    }

    constexpr T finalize() const
    {
        return m_value ^ xorValue;
    }

    static std::string toStdString(T value)
    {
        std::stringstream ss;
        ss << std::setfill('0') <<  std::setw(sizeof(T) * 2) << std::hex << value;
        return ss.str();
    }

private:
    const T m_value;
};

using Hasher = Crc<uint64_t, UINT64_C(0x42F0E1EBA9EA3693), 0, 0>;

} // namespace detail

/**
 * Calculates the hash of the type, passed as a template parameter.
 *
 * For example, hash can be used to check compatibility between client and server. Hashing takes
 * into account:
 * - field names
 * - field types (to maintain UBJSON compatibility)
 * - field order
 *
 * Note that:
 * - type may be a struct that uses other instrumented structs through inheritance and/or
 *     aggregation
 * - type may be non-instrumented at all
 * - generally, types that are supported by <code>nx::reflect::json</code> may be used with hash
 */
template<typename T, typename Hasher = detail::Hasher>
std::string hash()
{
    constexpr auto hash = detail::typeName<Hasher, T>(Hasher()).finalize();
    return Hasher::toStdString(hash);
}

} // namespace nx::reflect
