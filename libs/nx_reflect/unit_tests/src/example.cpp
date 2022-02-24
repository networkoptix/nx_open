// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <map>
#include <string>

#include <nx/reflect/instrument.h>

namespace nx::reflect::test {

namespace {

struct Address
{
    std::string city;
    std::string streetAddress;
    int zip = -1;
};

// Intrumenting must happen in the same namespace as the instrumented type.
NX_REFLECTION_INSTRUMENT(Address, (city)(streetAddress)(zip))

//-------------------------------------------------------------------------------------------------

//NX_ENUM_CLASS(Race,
//    caucasian,
//    african,
//    unknown)

enum class Race
{
    caucasian,
    african,
    unknown,
};

std::string toString(Race race)
{
    switch (race)
    {
        case Race::caucasian: return "caucasian";
        case Race::african: return "african";
        case Race::unknown: return "unknown";
    }
    return "invalid";
}

bool fromString(const std::string& str, Race* value)
{
    if (str == "caucasian")
        *value = Race::caucasian;
    else if (str == "african")
        *value = Race::african;
    else if (str == "unknown")
        *value = Race::unknown;
    else
        return false;

    return true;
}

//-------------------------------------------------------------------------------------------------

struct Person
{
    std::string name;
    int age = -1;
    Address address;
    Race race = Race::unknown;
    std::map<std::string, std::string> properties;
};

NX_REFLECTION_INSTRUMENT(Person, (name)(age)(address)(race)(properties))

bool operator==(const Person& left, const Person& right)
{
    return left.name == right.name
        && left.age == right.age
        && left.race == right.race
        && left.properties == right.properties;
}

} // namespace

} // namespace nx::reflect::test

//-------------------------------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <nx/reflect/json/serializer.h>
#include <nx/reflect/json/deserializer.h>

namespace nx::reflect::test {

TEST(example, json)
{
    Person person;
    person.name = "Vlad";
    person.age = 140;
    person.address.city = "Moscow";
    person.address.streetAddress = "Red square, apt. 1";
    person.address.zip = 111111;
    person.race = Race::caucasian;
    person.properties["dead"] = "true";

    const auto serialized = nx::reflect::json::serialize(person);
    const auto expected =
        "{\"name\":\"Vlad\",\"age\":140,\"address\":"
        "{\"city\":\"Moscow\",\"streetAddress\":\"Red square, apt. 1\",\"zip\":111111},"
        "\"race\":\"caucasian\","
        "\"properties\":{\"dead\":\"true\"}}";
    ASSERT_EQ(expected, serialized);

    Person deserializedPerson;
    ASSERT_TRUE(json::deserialize(serialized, &deserializedPerson));
    ASSERT_EQ(person, deserializedPerson);
}

//-------------------------------------------------------------------------------------------------
// Example of simple fusion expansion.

namespace {

class FieldEnumerator:
    public GenericVisitor<FieldEnumerator>
{
public:
    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        m_fieldNames.push_back(field.name());
    }

    std::vector<std::string> finish()
    {
        return std::exchange(m_fieldNames, {});
    }

private:
    std::vector<std::string> m_fieldNames;
};

template<typename T>
// requires nx::reflect::IsInstrumentedV<T>
static std::vector<std::string> enumerateFields()
{
    FieldEnumerator fieldEnumerator;
    return nx::reflect::visitAllFields<T>(fieldEnumerator);
}

} // namespace

TEST(example, print_struct_field_names)
{
    const auto fieldNames = enumerateFields<Person>();
    ASSERT_EQ(
        std::vector<std::string>({"name", "age", "address", "race", "properties"}),
        fieldNames);
}

} // namespace nx::reflect::test
