// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/reflect/urlencoded.h>

namespace nx::reflect::test {

struct PrivateConstructor
{
    std::string dummy;
    PrivateConstructor(std::string dummy): dummy(std::move(dummy)) {}

    template<typename T>
    friend constexpr T nx::reflect::createDefault();

private:
    PrivateConstructor(): dummy("") {}
};
NX_REFLECTION_INSTRUMENT(PrivateConstructor, (dummy))

struct PrivateConstructorField
{
    PrivateConstructor field;
    std::optional<PrivateConstructor> optionalField;
    std::vector<PrivateConstructor> vectorField;
    std::map<std::string, PrivateConstructor> mapField;
    PrivateConstructorField(std::string dummy): field(std::move(dummy)) {}

    template<typename T>
    friend constexpr T nx::reflect::createDefault();

private:
    PrivateConstructorField(): PrivateConstructorField("") {}
};
NX_REFLECTION_INSTRUMENT(PrivateConstructorField, (field)(optionalField)(vectorField)(mapField))

TEST(PrivateConstructorTest, Fixture)
{
    {
        const auto serialized = urlencoded::serialize(PrivateConstructorField{""});
        urlencoded::deserialize<PrivateConstructorField>(serialized);
    }
    {
        const auto serialized =
            urlencoded::serialize(std::map<std::string, PrivateConstructorField>{{"", {""}}});
        urlencoded::deserialize<PrivateConstructorField>(serialized);
    }
    {
        const auto serialized = urlencoded::serialize(std::vector<PrivateConstructorField>{{""}});
        urlencoded::deserialize<PrivateConstructorField>(serialized);
    }
    {
        const auto serialized = json::serialize(PrivateConstructorField{""});
        json::deserialize<PrivateConstructorField>(serialized);
    }
    {
        const auto serialized =
            json::serialize(std::map<std::string, PrivateConstructorField>{{"", {""}}});
        json::deserialize<PrivateConstructorField>(serialized);
    }
    {
        const auto serialized = json::serialize(std::vector<PrivateConstructorField>{{""}});
        json::deserialize<PrivateConstructorField>(serialized);
    }
}

} // namespace nx::reflect::test
