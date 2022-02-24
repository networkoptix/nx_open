# Urlencoded format {#nx_reflect_urlencoded}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

## Urlencoded serialization/deserialization example

    #include <iostream>
    #include <string>

    #include <nx/reflect/instrument.h>
    #include <nx/reflect/urlencoded.h>

    struct Foo
    {
        int number = 45;
        std::string text = "Forty-five";
    };

    // Making the structure reflectable.
    NX_REFLECTION_INSTRUMENT(Foo, (number)(text));

    int main()
    {
        const std::string serialized = nx::reflect::urlencoded::serialize(Foo{34, "Thirty-four"});
        std::cout<<serialized<<std::endl<<std::endl;

        const auto& [deserialized, deserializationSucceeded] =
            nx::reflect::urlencoded::deserialize<Foo>(serialized);
        std::cout<<"deserialized (result "<<(deserializationSucceeded ? "true" : "false")<<"):"<<std::endl<<
            "    "<<deserialized.number<<std::endl<<
            "    "<<deserialized.text<<std::endl<<
            std::endl;
        return 0;
    }

The program output:

    number=34&text=Thirty-four

    deserialized (result true):
        34
        Thirty-four

## Supported types

The following types are supported as field types:
- built-in numeric and float types
- std::string, std::string_view
- any STL containers
