# JSON format {#nx_reflect_json}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

## JSON serialization/deserialization example

    #include <iostream>
    #include <vector>

    #include <nx/reflect/instrument.h>
    #include <nx/reflect/json.h>

    struct Foo
    {
        int number = 45;
        std::string text = "Forty-five";
    };

    // Making the structure reflectable.
    NX_REFLECTION_INSTRUMENT(Foo, (number)(text));

    int main()
    {
        const std::string serialized = nx::reflect::json::serialize(Foo{34, "Thirty-four"});
        std::cout<<serialized<<std::endl<<std::endl;

        const auto& [deserialized, deserializationSucceeded] =
            nx::reflect::json::deserialize<Foo>(serialized);
        std::cout<<"deserialized (result "<<(deserializationSucceeded ? "true" : "false")<<"):"<<std::endl<<
            "    "<<deserialized.number<<std::endl<<
            "    "<<deserialized.text<<std::endl<<
            std::endl;
        return 0;
    }

The program output:

    {"number":34,"text":"Thirty-four"}

    deserialized (result true):
        34
        Thirty-four

## Supported types

The following types are supported as field types:
- built-in numeric and float types
- std::string, std::string_view
- std::chrono::*
- any STL containers
- types that can be converted to/from strings as described below

### Converting custom types to/from string
To convert a type T to string serializer requires it to satisfy `ConvertibleToString`.
Type T satisfies `ConvertibleToString` if it provides any of the following (in the order of preference):
- Member functions:
  * `T::operator std::string() const;`
  * `std::string T::toStdString() const;`
  * `ConvertibleToString T::toString() const; // returns a type different from T.`
- Non-member functions (found in the same namespace as T):
  * `ConvertibleToString toString(const T&);  // returns a type different from T.`
  * `void toString(const T&, std::string* str);`

### Tags that effect serialization/deserialiation
- `jsonSerializeChronoDurationAsNumber`. Causes `std::chrono::duration` types to be serialized
as numeric values, not strings. Example:
```
NX_REFLECTION_TAG_TYPE(Foo, jsonSerializeChronoDurationAsNumber)
```
As a result, every `std::chrono::duration` field of the `struct Foo` will be serialized as a number.

- `jsonSerializeInt64AsString`. Causes 64 bit integer types (like `long long`, `uint64_t` etc) to
be serialized as string values, not numbers. Example:
```
NX_REFLECTION_TAG_TYPE(Foo, jsonSerializeInt64AsString)
```
As a result, every 64 bit integer field of the `struct Foo` will be serialized as a string.

- - -

To convert a type T from string serializer requires it to satisfy `ConvertibleFromString`.
Type T satisfies `ConvertibleFromString` if it provides any of the following (in the order of preference):
- Member functions:
  * `static T T::fromStdString(const std::string_view&);`
  * `static T T::fromString(const std::string_view&);`
- Non-member functions (found in the same namespace as T):
  * `bool fromString(const std::string_view&, T*);`

## Providing custom serialization/deserialization functions
Sometimes, it is needed to make resulting JSON look different from what nx::reflect::json offers.
A custom functions that is found through ADL may be provided.

    #include <iostream>
    #include <vector>

    #include <nx/reflect/instrument.h>
    #include <nx/reflect/json.h>

    // Note that this type is NOT instrumented.
    struct CustomSerializable { int y = 0; };

    static void serialize(
        nx::reflect::json::SerializationContext* ctx,
        const CustomSerializable& value)
    {
        ctx->composer.writeInt(value.y);
    }

    static bool deserialize(
        const nx::reflect::json::DeserializationContext& ctx,
        CustomSerializable* data)
    {
        return nx::reflect::json::deserialize(ctx, &data->y);
    }

    // Instrumented structure.
    struct Foo
    {
        int number = 45;
        CustomSerializable c;
    };

    // Making the structure reflectable.
    NX_REFLECTION_INSTRUMENT(Foo, (number)(c));

    int main()
    {
        std::cout<<nx::reflect::json::serialize(Foo{34, {45}})<<std::endl;
        return 0;
    }

Program output:

    {"number":34,"c":45}

`ctx->composer` is `nx::reflect::AbstractComposer*`.

NOTE: If type `CustomSerializable` would have been instrumented and custom function not provided,
the output would look like the following:

    {"number":34,"c":{"y":45}}
