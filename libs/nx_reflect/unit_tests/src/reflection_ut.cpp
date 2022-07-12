// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/generic_visitor.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/field_enumerator.h>
#include <nx/reflect/json/serializer.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/type_utils.h>

#include "types.h"

namespace nx::reflect::test {

TEST(Reflection, is_instrumented)
{
    static_assert(IsInstrumentedV<A>);
    static_assert(IsInstrumentedV<BB::B>);

    static_assert(!IsInstrumentedV<std::vector<A>>);
    static_assert(!IsInstrumentedV<C>);
    static_assert(!IsInstrumentedV<int>);
    static_assert(!IsInstrumentedV<std::vector<int>>);
}

//-------------------------------------------------------------------------------------------------

TEST(Reflection, field_traverse)
{
    const auto fieldNames = nx::reflect::listFieldNames<A>();
    const auto expected = std::array<std::string_view, 6>{"num", "str", "b", "bs", "c", "keyToValue"};
    ASSERT_EQ(expected, fieldNames);
}

//-------------------------------------------------------------------------------------------------

class FooWithGetSetProperty
{
public:
    std::string str() const { return m_str; }
    void setStr(const std::string& s) { m_str = s; }

    int n() const { return m_n; }
    void setN(int n) { m_n = n; }

    bool operator==(const FooWithGetSetProperty& right) const
    {
        return m_str == right.m_str
            && m_n == right.m_n;
    }

private:
    int m_n = 0;
    std::string m_str;
};

NX_REFLECTION_INSTRUMENT_GSN(
    FooWithGetSetProperty,
    ((&FooWithGetSetProperty::n, &FooWithGetSetProperty::setN, "intVal"))
    ((&FooWithGetSetProperty::str, &FooWithGetSetProperty::setStr, "strVal")))

TEST(Reflection, struct_properties_represented_with_get_set_methods)
{
    const auto fieldNames = nx::reflect::listFieldNames<FooWithGetSetProperty>();
    const auto expected = std::array<std::string_view, 2>{"intVal", "strVal"};
    ASSERT_EQ(expected, fieldNames);
}

TEST(Reflection, struct_properties_represented_with_get_set_methods_json)
{
    FooWithGetSetProperty foo;
    foo.setN(45);
    foo.setStr("abc");

    const auto serialized = nx::reflect::json::serialize(foo);
    ASSERT_EQ(R"({"intVal":45,"strVal":"abc"})", serialized);

    FooWithGetSetProperty deserialized;
    ASSERT_TRUE(nx::reflect::json::deserialize(serialized, &deserialized));

    ASSERT_EQ(foo, deserialized);
}

//-------------------------------------------------------------------------------------------------

class FooWithReadOnlyProperty
{
public:
    std::string str() const { return m_str; }
    void setStr(const std::string& s) { m_str = s; }

    std::string readOnlyProperty() const { return "readOnly"; }

    bool operator==(const FooWithReadOnlyProperty& right) const = default;

private:
    std::string m_str;
};

NX_REFLECTION_INSTRUMENT_GSN(
    FooWithReadOnlyProperty,
    ((&FooWithReadOnlyProperty::str, &FooWithReadOnlyProperty::setStr, "strVal"))
    ((&FooWithReadOnlyProperty::readOnlyProperty, nx::reflect::none, "ro")))

TEST(Reflection, struct_properties_read_only)
{
    const auto fieldNames = nx::reflect::listFieldNames<FooWithReadOnlyProperty>();
    const auto expected = std::array<std::string_view, 2>{"strVal", "ro"};
    ASSERT_EQ(expected, fieldNames);
}

TEST(Reflection, struct_properties_read_only_json)
{
    FooWithReadOnlyProperty foo;
    foo.setStr("abc");

    const auto serialized = nx::reflect::json::serialize(foo);
    ASSERT_EQ(R"({"strVal":"abc","ro":"readOnly"})", serialized);
}

//-------------------------------------------------------------------------------------------------

class FooWithWriteOnlyProperty
{
public:
    std::string str() const { return m_str; }
    void setStr(const std::string& s) { m_str = s; }

    void setWriteOnlyProperty(const std::string& val) { m_wo = val; }

    bool operator==(const FooWithWriteOnlyProperty& right) const = default;

private:
    std::string m_str;
    std::string m_wo;
};

NX_REFLECTION_INSTRUMENT_GSN(
    FooWithWriteOnlyProperty,
    ((&FooWithWriteOnlyProperty::str, &FooWithWriteOnlyProperty::setStr, "strVal"))
    ((nx::reflect::none, &FooWithWriteOnlyProperty::setWriteOnlyProperty, "wo")))

TEST(Reflection, struct_properties_write_only)
{
    const auto fieldNames = nx::reflect::listFieldNames<FooWithWriteOnlyProperty>();
    const auto expected = std::array<std::string_view, 2>{"strVal", "wo"};
    ASSERT_EQ(expected, fieldNames);
}

TEST(Reflection, struct_properties_write_only_json)
{
    FooWithWriteOnlyProperty foo;
    foo.setStr("abc");
    foo.setWriteOnlyProperty("write only");

    FooWithWriteOnlyProperty deserialized;
    ASSERT_TRUE(nx::reflect::json::deserialize(R"({"strVal":"abc","wo":"write only"})", &deserialized));

    ASSERT_EQ(foo, deserialized);
}

//-------------------------------------------------------------------------------------------------

template<typename T>
struct GenericFoo
{
    T param;
};

NX_REFLECTION_INSTRUMENT_TEMPLATE(GenericFoo, (param))

struct Bar
{
    std::string text;
};

NX_REFLECTION_INSTRUMENT(Bar, (text))

struct Bor
{
    int n;
};

NX_REFLECTION_INSTRUMENT(Bor, (n))

TEST(Reflection, instrumenting_template_struct)
{
    ASSERT_EQ(
        R"({"param":{"text":"Hello, world"}})",
        json::serialize(GenericFoo<Bar>{Bar{"Hello, world"}}));

    ASSERT_EQ(R"({"param":{"n":45}})", json::serialize(GenericFoo<Bor>{Bor{45}}));
}

//-------------------------------------------------------------------------------------------------

template<typename T>
struct GenericFooDescendant: GenericFoo<T>
{
    // This alias is required for the base type fields to be found by the reflection.
    using base_type = GenericFoo<T>;

    std::string ownWord;
};

NX_REFLECTION_INSTRUMENT_TEMPLATE(GenericFooDescendant, (ownWord))

TEST(Reflection, instrumenting_descendant_template_struct)
{
    ASSERT_EQ(
        R"({"param":{"text":"Hello, world"},"ownWord":"Ciao mondo"})",
        json::serialize(GenericFooDescendant<Bar>{{Bar{"Hello, world"}}, "Ciao mondo"}));

    ASSERT_EQ(
        R"({"param":{"n":45},"ownWord":"Ciao mondo"})",
        json::serialize(GenericFooDescendant<Bor>{{Bor{45}}, "Ciao mondo"}));
}

//-------------------------------------------------------------------------------------------------

template<typename T>
struct GenericFooDescendantWithRegularBase: Bar
{
    // This alias is required for the base type fields to be found by the reflection.
    using base_type = Bar;

    T own;
};

NX_REFLECTION_INSTRUMENT_TEMPLATE(GenericFooDescendantWithRegularBase, (own))

TEST(Reflection, instrumenting_descendant_template_struct_with_non_template_base)
{
    ASSERT_EQ(
        R"({"text":"Hello, world","own":{"n":45}})",
        json::serialize(GenericFooDescendantWithRegularBase<Bor>{{Bar{"Hello, world"}}, {45}}));
}

//-------------------------------------------------------------------------------------------------

template<typename T, int n>
struct AdvancedFoo
{
    int bar;
};

// Reflect manually
template<typename T, int n, typename Visitor, typename... Members>
auto nxReflectVisitAllFields(
    AdvancedFoo<T, n>* tag,
    Visitor&& visitor,
    Members&&... members)
{
    using Self = AdvancedFoo<T, n>;
    return nxReflectVisitAllFields(
        detail::toBase(tag),
        detail::forward<Visitor>(visitor),
        WrappedMemberVariableField("bar", &Self::bar),
        detail::forward<Members>(members)...);
}

TEST(Reflection, instrumenting_template_with_non_type_parameters_manually)
{
    ASSERT_EQ(
        R"({"bar":4})",
        json::serialize(AdvancedFoo<Bor, 3>{4}));
}

template<typename T>
struct AdvancedFoo<T, 2>: T
{
    using base_type = T;

    int bar;
    int x;

    int getY() const { return 111; }
    void setY(float) {}
};

// Reflect manually
template<typename T, typename Visitor, typename... Members>
auto nxReflectVisitAllFields(
    AdvancedFoo<T, 2>* tag,
    Visitor&& visitor,
    Members&&... members)
{
    using Self = AdvancedFoo<T, 2>;
    return nxReflectVisitAllFields(
        detail::toBase(tag),
        detail::forward<Visitor>(visitor),
        WrappedMemberVariableField("bar", &Self::bar),
        WrappedMemberVariableField("x", &Self::x),
        WrappedProperty(&Self::getY, &Self::setY, "y"),
        detail::forward<Members>(members)...);
}

TEST(Reflection, instrumenting_complex_template_specialization_manually)
{
    ASSERT_EQ(
        R"({"n":1,"bar":2,"x":3,"y":111})",
        json::serialize(AdvancedFoo<Bor, 2>{{1}, 2, 3}));
}

//-------------------------------------------------------------------------------------------------

struct NonInstrumentedDescendant: Bar { int foo = 0; };

template<typename T>
struct NonInstrumentedGenericFooDescendant: GenericFoo<T>
{
    // This alias is required for the base type fields to be found by the reflection.
    using base_type = GenericFoo<T>;

    std::string ownWord;
};

TEST(Reflection, non_instrumented_child_of_instrumented_base_is_converted_to_base)
{
    static_assert(nx::reflect::IsInstrumentedV<NonInstrumentedDescendant>);
    static_assert(nx::reflect::IsInstrumentedV<NonInstrumentedGenericFooDescendant<int>>);
}

} // namespace nx::reflect::test
