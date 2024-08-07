# nx_reflect. Reflection & serialization library {#nx_reflect}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

[TOC]

## Introduction

This library is a simple compile-time reflection API. For example, it provides a way to iterate
over structure fields at compile-time.

This is used to implement serialization formats like JSON. Example:

    #include <iostream>
    #include <vector>

    #include <nx/reflect/instrument.h>
    #include <nx/reflect/json.h>

    struct Foo
    {
        int number = 45;
        std::string text = "Forty-five";
    };

    // In the absense of reflection support in C++ a type has to be "instrumented" to be available
    // through the reflection API.
    NX_REFLECTION_INSTRUMENT(Foo, (number)(text));

    int main()
    {
        // nx::reflect::json::serialize uses nx_reflect API to build JSON representation of the
        // given structure.
        std::cout<<nx::reflect::json::serialize(Foo())<<std::endl;
        return 0;
    }

The program output:

    {"number":45,"text":"Forty-five"}


## A more generic example (enumerating structure's fields)

    #include <algorithm>
    #include <iostream>
    #include <vector>

    #include <nx/reflect/instrument.h>

    #include <nx/reflect/generic_visitor.h>

    // Regular structure.
    struct Foo
    {
        int number = 45;
        std::string text = "Forty-five";
    };

    // Making the structure reflectable.
    NX_REFLECTION_INSTRUMENT(Foo, (number)(text));

    // Field enumerator that uses this library to list instrumented structure fields.
    class FieldEnumerator:
        public nx::reflect::GenericVisitor<FieldEnumerator>
    {
    public:
        template<typename WrappedField>
        void visitField(const WrappedField& field)
        {
            // This method is invoked once per each field of the struct.
            m_fieldNames.push_back(field.name());
        }

        std::vector<std::string> finish()
        {
            // Invoked at the very end.
            return std::exchange(m_fieldNames, {});
        }

    private:
        std::vector<std::string> m_fieldNames;
    };

    int main()
    {
        FieldEnumerator fieldEnumerator;
        // FieldEnumerator::finish() return type is propagated here.
        const auto fieldNames = nx::reflect::visitAllFields<Foo>(fieldEnumerator);
        std::for_each(fieldNames.begin(), fieldNames.end(),
            [](auto& name) { std::cout<<name<<std::endl; });
        return 0;
    }

The program output:

    number
    text

## Implementation goals

This library tends to achieve the following:

### Not pay for what you don't use

- Instrumentation should have no compile-time or runtime overhead when it is not used. Only when
it is explicitly used (e.g., to serialize an object to JSON) it can consume compile- or run- time.
- <nx/reflect/instrument.h> should seek to minimize dependencies to reduce compilation overhead.
- Instrumentation macro generates template functions that are instantiated only when needed (e.g.,
when serializing a structure into JSON), avoiding unnecessary symbol generation and compile time.

### Decouple Instrumentation and Usage

- *Single Instrumentation*: Types are instrumented once, irrespective of how they are used (e.g.,
JSON, XML serialization). This ensures no changes in the instrumentation code are needed when new
serialization formats are added.

### Prefer Compile-Time Reflection

The approach aligns with C++ reflection likely coming with C++26.<br/>
Adhering to compile-time reflection principles and decoupling instrumentation from its
applications will simplify migration to future C++26 reflection.<br/>
The migration will likely involve updating serialization functions and removing
NX_REFLECTION_INSTRUMENT macros, with minimal or no changes to existing code that uses nx_reflect.

### Simplify public API where appropriate

The C++ reflection is always complicated: it requires tricky macro and sophisticated template
metaprogramming. As a result, when someone runs into a problem while using the reflection (a compile
error), it is not easy to understand what is wrong.

The following was considered to reduce the added complexity to some degree at least:

- *Non-Conditional API*: Provide a single function or macro for each feature. Adding
overloads/variants should be carefully considered.
- *Minimal Macro Usage*: While macros enhance convenience, their use is minimized to avoid common
pitfalls like hard-to-read code and debugging difficulties.
- *Focused Implementation*: Separate instrumentation from its applications, ensuring each is
minimalistic and straightforward.
- *Single Entry Point*: Each serialization format should have one entry point (e.g.,
`nx::reflect::json::serialize`), avoiding potential conflicts and reducing compilation time.


## Instrumenting

In different cases, a different macro is used to instrument a type.<br/>
A non-template structure:

    #include <string>

    #include <nx/reflect/instrument.h>

    struct Foo
    {
        int number = 45;
        std::string text = "Forty-five";
    };

    // Making the structure reflectable.
    NX_REFLECTION_INSTRUMENT(Foo, (number)(text));

- - -

A descendant structure:

    #include <string>

    #include <nx/reflect/instrument.h>

    struct Base
    {
        int number = 45;
        std::string text = "Forty-five";
    };

    NX_REFLECTION_INSTRUMENT(Base, (number)(text));

    // This child wants only Base::text field to be visible through type Child1.
    struct Child1: Base
    {
        std::string bar;
    };

    NX_REFLECTION_INSTRUMENT(Child1, (text)(bar));

    // nx::reflect::json::serialize(Child1()) returns {"text": ..., "bar": ...}

    // This child wants all Base fields to be visible through type Child2.
    struct Child2: Base
    {
        // Reflection uses this alias to find the base and its fields.
        using base_type = Base;

        std::string bar;
    };

    NX_REFLECTION_INSTRUMENT(Child2, (bar));

    // nx::reflect::json::serialize(Child2()) returns {"number": ..., "text": ..., "bar": ...}

- - -

Instrumenting template structure:

    #include <nx/reflect/instrument.h>

    template<typename T>
    struct GenericFoo
    {
        int n;
        T t;
    };

    NX_REFLECTION_INSTRUMENT_TEMPLATE(GenericFoo, (n)(t))

    // nx::reflect::json::serialize(GenericFoo<int>{3, 4}) returns {"n": 3, "t": 4}
    // nx::reflect::json::serialize(GenericFoo<std::string>{3, {"four"}}) returns {"n": 3, "t": "four"}

NOTE: Only an instantiated GenericFoo<> can be used to access fields through reflection.

- - -

Instrumenting type that uses get/set methods to promote its attributes:

    #include <string>

    #include <nx/reflect/instrument.h>

    class FooWithGetSetProperty
    {
    public:
        std::string str() const { return m_str; }
        void setStr(const std::string& s) { m_str = s; }

        bool operator==(const FooWithGetSetProperty& right) const { return m_str == right.m_str; }

    private:
        std::string m_str = "Hello";
    };

    // Attribute names have to be provided explicitly.
    NX_REFLECTION_INSTRUMENT_GSN(
        FooWithGetSetProperty,
        ((&FooWithGetSetProperty::str, &FooWithGetSetProperty::setStr, "strAttribute")))

    // nx::reflect::json::serialize(FooWithGetSetProperty()) returns {"strAttribute":"Hello"}

    Note that `nx::reflect::none` can be used in `NX_REFLECTION_INSTRUMENT_GSN` macro instead of a
    getter or setter to declare write-only or read-only property respectively.
    In this case, only deserialization is available for a type with write-only property
    and only serialization for a type with read-only property.
    An attempt to deserialize to a value of type with a read-only property causes compile error.

NOTE: Class hierarchy may contain types instrumented in different ways.

## Instrumenting enums
Enum can be instrumented using the following macro:

    #include <nx/reflect/enum_instrument.h>

    enum class Permission { read, write, execute };
    NX_REFLECTION_INSTRUMENT_ENUM(Permission, read, write, execute)

After that it is possible to convert this enum to/from string using standard nx_reflect functions.

    #include <nx/reflect/to_string.h>
    #include <nx/reflect/from_string.h>

    void foo()
    {
        const std::string str = nx::reflect::toString(Permission::write); //< str == "write"
        Permission perm;
        nx::reflect::fromString(str, &perm); //< perm == Permission::write
    }

Of course it is possible to use such enum in instrumented structures without need to write
toString/fromString.

    struct Foo
    {
        Permission permission;
    };
    NX_REFLECTION_INSTRUMENT(Foo, (permission))

    // nx::reflect::json::serialize(Foo{Permission::write}) returns {"permission": "write"}

Also nx_reflect provides macros which declare and instrument enum in one turn.

    #include <nx/reflect/enum_instrument.h>

    // enum Permission { read, write, execute };
    NX_REFLECTION_ENUM(Permission, read, write, execute)

    // enum class Permission { read, write, execute };
    NX_REFLECTION_ENUM_CLASS(Permission, read, write, execute)

Due to technical limitations, enums declared **inside a class or struct** have to be declared with
another macros (suffixed with `_IN_CLASS`).

    #include <nx/reflect/enum_instrument.h>

    struct Foo
    {
        // enum Permission { read, write, execute };
        NX_REFLECTION_ENUM_IN_CLASS(Permission, read, write, execute)

        // enum class Permission { read, write, execute };
        NX_REFLECTION_ENUM_CLASS_IN_CLASS(Permission, read, write, execute)
    };

All declaration macros support enums with explicit constant values.

    #include <nx/reflect/enum_instrument.h>

    // enum class Permission { read, write, execute };
    NX_REFLECTION_ENUM_CLASS(Permission, read = 0x1, write = 0x2, execute = 0x4)

## Supported serialization formats

@subpage nx_reflect_json
@subpage nx_reflect_urlencoded
