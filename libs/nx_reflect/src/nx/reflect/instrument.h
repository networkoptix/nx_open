// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

// TODO: #akolesnikov Implement required macro locally to reduce dependencies.
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>

#include "enum_instrument.h"
#include "tags.h"
#include "utils.h"

namespace nx::reflect {

/**
 * Wraps a structure member-variable structure.
 */
template<typename Class, typename Field>
class WrappedMemberVariableField
{
public:
    using Type = Field;

    constexpr WrappedMemberVariableField(
        const char* name,
        Field Class::*fieldPtr)
        :
        m_name(name),
        m_fieldPtr(fieldPtr)
    {
    }

    constexpr const char* name() const { return m_name; }

    template<typename T>
    void set(Class* obj, T&& arg) const
    {
        obj->*m_fieldPtr = detail::forward<T>(arg);
    }

    auto get(const Class& obj) const
    {
        return obj.*m_fieldPtr;
    }

    Field Class::* ptr() const { return m_fieldPtr; }

private:
    const char* m_name = nullptr;
    Field Class::* m_fieldPtr = nullptr;
};

//-------------------------------------------------------------------------------------------------

/**
 * Wraps a class property represented by set and get member functions.
 */
template<typename Getter, typename Setter>
class WrappedProperty
{
public:
    using Type = typename detail::GetterReturnType<Getter>::Type;

    WrappedProperty(Getter getter, Setter setter, const char* name):
        m_name(name), m_getter(getter), m_setter(setter) {}

    const char* name() const { return m_name; }

    template<typename Class, typename Arg>
    void set(Class* obj, Arg&& arg) const { (obj->*m_setter)(detail::forward<Arg>(arg)); }

    template<typename Class>
    auto get(const Class& obj) const { return (obj.*m_getter)(); }

private:
    const char* m_name = nullptr;
    Getter m_getter = nullptr;
    Setter m_setter = nullptr;
};

/**
 * Can be used to specify and absent setter or getter.
 * It allows instrumenting structures with read-only or write-only properties.
 */
struct None {};
static constexpr None none{};

/**
 * Wraps a read-only property represented by get member function.
 */
template<typename Getter>
class WrappedProperty<Getter, None>
{
public:
    using Type = typename detail::GetterReturnType<Getter>::Type;

    WrappedProperty(Getter getter, const None& /*dummy*/, const char* name):
        m_name(name), m_getter(getter) {}

    const char* name() const { return m_name; }

    template<typename Class>
    auto get(const Class& obj) const { return (obj.*m_getter)(); }

private:
    const char* m_name = nullptr;
    Getter m_getter = nullptr;
};

/**
 * Wraps a write-only property represented by set member function.
 */
template<typename Setter>
class WrappedProperty<None, Setter>
{
public:
    using Type = typename detail::SetterReturnType<Setter>::Type;

    WrappedProperty(const None& /*dummy*/, Setter setter, const char* name):
        m_name(name), m_setter(setter) {}

    const char* name() const { return m_name; }

    template<typename Class, typename Arg>
    void set(Class* obj, Arg&& arg) const { (obj->*m_setter)(detail::forward<Arg>(arg)); }

private:
    const char* m_name = nullptr;
    Setter m_setter = nullptr;
};

//-------------------------------------------------------------------------------------------------

namespace detail {

struct NoBase {};

template<typename Visitor, typename... Members>
constexpr auto nxReflectVisitAllFields(NoBase*, Visitor&& visitor, Members&&... members)
{
    return detail::forward<Visitor>(visitor)(detail::forward<Members>(members)...);
}

template<typename T, typename = void>
struct BaseOf
{
    using Type = NoBase;
};

template<typename T>
struct BaseOf<T, detail::void_t<typename T::base_type>>
{
    using Type = typename T::base_type;
};

template<typename T>
constexpr typename BaseOf<T>::Type* toBase(T*)
{
    return nullptr;
}

} // namespace detail

template<typename Data, typename Visitor>
constexpr auto visitAllFields(Visitor&& visit)
{
    return nxReflectVisitAllFields((Data*) nullptr, detail::forward<Visitor>(visit));
}

namespace detail {

struct DummyVisitor
{
    template <typename... Members>
    void operator()(Members&&...) const
    {
    }
};

constexpr DummyVisitor dummyVisitor;

} // namespace detail

/**
 * Checks whether the T is instrumented:
 * if constexpr (nx::reflect::IsInstrumented<Foo>::value) ...
 */
template<typename T, typename = void>
struct IsInstrumented
{
    static constexpr bool value = false;
};

template<typename T>
struct IsInstrumented<T, detail::void_t<decltype(nxReflectVisitAllFields((T*) nullptr, detail::dummyVisitor))>>
{
    static constexpr bool value = true;
};

template<typename... U>
constexpr bool IsInstrumentedV = IsInstrumented<U...>::value;

} // namespace nx::reflect

//-------------------------------------------------------------------------------------------------

#define NX_REFLECTION_EXPAND_FIELDS(_, SELF, FIELD) \
    ::nx::reflect::WrappedMemberVariableField(BOOST_PP_STRINGIZE(FIELD), &SELF::FIELD),

/**
 * Instruments structure that promotes its fields as member-variables.
 * Example:
 * <pre><code>
 * struct Foo { int val; std::string str; };
 *
 * NX_REFLECTION_INSTRUMENT(Foo, (val)(str))
 * </code></pre>
 *
 * NOTE: If instrumented type has base_type alias that points to instrumented base,
 * that base fields will be reported.
 */
#define NX_REFLECTION_INSTRUMENT(SELF, FIELDS) \
    template<typename Visitor, typename... Members> \
    constexpr auto nxReflectVisitAllFields(SELF* tag, Visitor&& visit, Members&&... members) \
    { \
        return nxReflectVisitAllFields( \
            ::nx::reflect::detail::toBase(tag), \
            ::nx::reflect::detail::forward<Visitor>(visit), \
            BOOST_PP_SEQ_FOR_EACH(NX_REFLECTION_EXPAND_FIELDS, SELF, FIELDS) \
            ::nx::reflect::detail::forward<Members>(members)...); \
    }

//-------------------------------------------------------------------------------------------------

/**
 * Instruments template structure that promotes its fields as member-variables.
 * Example:
 * <pre><code>
 * template<typename T>
 * struct Foo { T t; std::string str; };
 *
 * NX_REFLECTION_INSTRUMENT_TEMPLATE(Foo, (t)(str))
 * </code></pre>
 *
 * NOTE: Visiting can only be done on instantiated template type.
 * NOTE: If instrumented type has base_type alias that points to instrumented base,
 * that base fields will be reported.
 */
#define NX_REFLECTION_INSTRUMENT_TEMPLATE(SELF, FIELDS) \
    template<typename... Args, typename Visitor, typename... Members> \
    auto nxReflectVisitAllFields(SELF<Args...>* tag, Visitor&& visit, Members&&... members) \
    { \
        return nxReflectVisitAllFields( \
            ::nx::reflect::detail::toBase(tag), \
            ::nx::reflect::detail::forward<Visitor>(visit), \
            BOOST_PP_SEQ_FOR_EACH(NX_REFLECTION_EXPAND_FIELDS, SELF<Args...>, FIELDS) \
            ::nx::reflect::detail::forward<Members>(members)...); \
    }

//-------------------------------------------------------------------------------------------------

#define NX_REFLECTION_EXPAND_GSN_FIELDS(_1, _2, FIELD) \
    ::nx::reflect::WrappedProperty FIELD,

/**
 * Instruments class that promotes its fields using get/set functions.
 * Example:
 * <pre><code>
 * class Foo
 * {
 * public:
 *     int val() const { return m_val; };
 *     void setVal(int v) { m_val = v; };
 * private:
 *     int m_val;
 * };
 *
 * NX_REFLECTION_INSTRUMENT_GSN(Foo, ((&Foo::val, &Foo::setVal, "value")))
 * </code></pre>
 *
 * NOTE: If instrumented type has base_type alias that points to instrumented base,
 * that base fields will be reported.
 */
#define NX_REFLECTION_INSTRUMENT_GSN(SELF, FIELDS) \
    template<typename Visitor, typename... Members> \
    auto nxReflectVisitAllFields(SELF* tag, Visitor&& visit, Members&&... members) \
    { \
        return nxReflectVisitAllFields( \
            ::nx::reflect::detail::toBase(tag), \
            ::nx::reflect::detail::forward<Visitor>(visit), \
            BOOST_PP_SEQ_FOR_EACH(NX_REFLECTION_EXPAND_GSN_FIELDS, _, FIELDS) \
            ::nx::reflect::detail::forward<Members>(members)...); \
    }

//-------------------------------------------------------------------------------------------------

/**
 * Add a tag to the specified type.
 * A tag presence may be checked by evaluating the constexpr boolean expression
 * <pre><code>TAG((const TYPE*) nullptr)</code></pre>.
 *
 * Effectively, this macro defines the following function:
 * <pre><code>static constexpr bool TAG(const TYPE*) { return true; }</code></pre>
 * A default overload like
 * <pre><code>template<typename T> static constexpr bool TAG(const T*) { return false; }</code></pre>
 * should be defined for the ADL to work.
 *
 * Note: This macro MUST BE placed next to the NX_REFLECTION_INSTRUMENT macro. Otherwise,
 * an undefined behavior may result due to different scopes of NX_REFLECTION_INSTRUMENT and
 * NX_REFLECTION_TAG_TYPE macros for the same type.
 */
#define NX_REFLECTION_TAG_TYPE(TYPE, TAG) \
    static constexpr bool TAG(const TYPE*) { return true; }

/**
 * Similar to NX_REFLECTION_TAG_TYPE. But, works for template types.
 */
#define NX_REFLECTION_TAG_TEMPLATE_TYPE(TEMPLATE_TYPE, TAG) \
    template<typename... Args> \
    static constexpr bool TAG(const TEMPLATE_TYPE<Args...>*) { return true; }
