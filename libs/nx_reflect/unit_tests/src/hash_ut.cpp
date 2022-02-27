// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include <nx/reflect/hash.h>
#include <nx/reflect/instrument.h>

using namespace nx::reflect;

namespace {

enum NonInstrumentedEnum
{
    NonInstrumentedEnumValue1,
    NonInstrumentedEnumValue2
};

enum class NonInstrumentedEnumClass
{
    NonInstrumentedEnumClassValue1,
    NonInstrumentedEnumClassValue2
};

NX_REFLECTION_ENUM(Enum, EnumValue1, EnumValue2)
NX_REFLECTION_ENUM_CLASS(EnumClass, EnumClassValue1, EnumClassValue2)

class ClassWithEnums
{
public:
    NX_REFLECTION_ENUM_IN_CLASS(EnumInClass, EnumInClassValue1, EnumInClassValue2)

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(EnumClassInClass,
        EnumClassInClassValue1,
        EnumClassInClassValue2)
};

struct SimpleStructure
{
    bool b;
    int8_t i8;
    uint8_t u8;
    int16_t i16;
    uint16_t u16;
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;
    float f;
    double d;
    std::string s;
    std::string_view sv;
    std::chrono::milliseconds ms;
    std::vector<bool> vec;
    std::vector<std::vector<std::string>> vecOfVec;
    std::map<int, std::string> m;
    std::unordered_map<std::string, int64_t> um;
    std::set<int32_t> st;
    std::unordered_set<uint16_t> uset;
    Enum e;
    EnumClass ec;
    ClassWithEnums::EnumInClass eic;
    ClassWithEnums::EnumClassInClass ecic;
    NonInstrumentedEnum nie;
    NonInstrumentedEnumClass niec;
};

NX_REFLECTION_INSTRUMENT(SimpleStructure,
        (b)
        (i8)
        (u8)
        (i16)
        (u16)
        (i32)
        (u32)
        (i64)
        (u64)
        (f)
        (d)
        (s)
        (sv)
        (ms)
        (vec)
        (vecOfVec)
        (m)
        (um)
        (st)
        (uset)
        (e)
        (ec)
        (eic)
        (ecic)
        (nie)
        (niec))

struct EncapsulatedStructure
{
    int x;
    SimpleStructure s;
    std::vector<SimpleStructure> vecOfS;
};

NX_REFLECTION_INSTRUMENT(EncapsulatedStructure, (x) (s) (vecOfS))

struct ExtendedStructure: public SimpleStructure
{
    int n;
};

NX_REFLECTION_INSTRUMENT(ExtendedStructure, (b) (s) (n))

using SimpleType = int;

using SimpleContainer = std::map<int, std::string>;

class TestHasher
{
public:
    TestHasher() = default;

    explicit TestHasher(const std::string& str): m_str(str) {}

    TestHasher add(const char* str) const
    {
        return TestHasher(m_str + str);
    }

    std::string finalize() const
    {
        return m_str;
    }

    static std::string toStdString(std::string str)
    {
        return str;
    }

private:
    const std::string m_str;
};

template<typename T>
std::string hashWithTestHasher()
{
    auto hash = detail::typeName<TestHasher, T>(TestHasher()).finalize();
    return TestHasher::toStdString(hash);
}

} // anonymous namespace

TEST(Hash, different_structures_have_expected_serialization)
{
    const char* serializedSimpleStructure =
        "{"
        "b:int64,"
        "i8:int64,"
        "u8:int64,"
        "i16:int64,"
        "u16:int64,"
        "i32:int64,"
        "u32:int64,"
        "i64:int64,"
        "u64:int64,"
        "f:double,"
        "d:double,"
        "s:string,"
        "sv:string,"
        "ms:int64,"
        "vec:[int64],"
        "vecOfVec:[[string]],"
        "m:[int64,string],"
        "um:[string,int64],"
        "st:[int64],"
        "uset:[int64],"
        "e:string,"
        "ec:string,"
        "eic:string,"
        "ecic:string,"
        "nie:int64,"
        "niec:int64,"
        "}";
    EXPECT_EQ(serializedSimpleStructure, hashWithTestHasher<SimpleStructure>());

    const char* serializedEncapsulatedStructure =
        "{"
        "x:int64,"
        "s:{"
            "b:int64,"
            "i8:int64,"
            "u8:int64,"
            "i16:int64,"
            "u16:int64,"
            "i32:int64,"
            "u32:int64,"
            "i64:int64,"
            "u64:int64,"
            "f:double,"
            "d:double,"
            "s:string,"
            "sv:string,"
            "ms:int64,"
            "vec:[int64],"
            "vecOfVec:[[string]],"
            "m:[int64,string],"
            "um:[string,int64],"
            "st:[int64],"
            "uset:[int64],"
            "e:string,"
            "ec:string,"
            "eic:string,"
            "ecic:string,"
            "nie:int64,"
            "niec:int64,"
        "},"
        "vecOfS:["
            "{"
                "b:int64,"
                "i8:int64,"
                "u8:int64,"
                "i16:int64,"
                "u16:int64,"
                "i32:int64,"
                "u32:int64,"
                "i64:int64,"
                "u64:int64,"
                "f:double,"
                "d:double,"
                "s:string,"
                "sv:string,"
                "ms:int64,"
                "vec:[int64],"
                "vecOfVec:[[string]],"
                "m:[int64,string],"
                "um:[string,int64],"
                "st:[int64],"
                "uset:[int64],"
                "e:string,"
                "ec:string,"
                "eic:string,"
                "ecic:string,"
                "nie:int64,"
                "niec:int64,"
            "}"
        "],"
        "}";
    EXPECT_EQ(serializedEncapsulatedStructure, hashWithTestHasher<EncapsulatedStructure>());

    const char* serializedExtendedStructure =
        "{"
        "b:int64,"
        "s:string,"
        "n:int64,"
        "}";
    EXPECT_EQ(serializedExtendedStructure, hashWithTestHasher<ExtendedStructure>());

    const char* serializedSimpleType = "int64";
    EXPECT_EQ(serializedSimpleType, hashWithTestHasher<SimpleType>());

    const char* serializedSimpleContainer =
        "["
        "int64,"
        "string"
        "]";
    EXPECT_EQ(serializedSimpleContainer, hashWithTestHasher<SimpleContainer>());
}

//-------------------------------------------------------------------------------------------------

TEST(Hash, crc_hash_is_correct)
{
    auto hash = detail::Hasher().finalize();
    EXPECT_EQ("0000000000000000", detail::Hasher::toStdString(hash));

    hash = detail::Hasher().add("The quick brown fox jumps over the lazy dog").finalize();
    EXPECT_EQ("41e05242ffa9883b", detail::Hasher::toStdString(hash));

    hash = detail::Hasher().add("The quick brown fox jumps over the lazy dog.").finalize();
    EXPECT_EQ("ea6939e68ade7f25", detail::Hasher::toStdString(hash));

    hash = detail::Hasher()
        .add("The ")
        .add("quick ")
        .add("brown ")
        .add("fox ")
        .add("jumps ")
        .add("over ")
        .add("the ")
        .add("lazy ")
        .add("dog.")
        .finalize();
    EXPECT_EQ("ea6939e68ade7f25", detail::Hasher::toStdString(hash));
}

//-------------------------------------------------------------------------------------------------

namespace {

std::set<std::string> generatedHashes;

template<typename T>
void assertUniqueHashIsGenerated()
{
    std::string hash = nx::reflect::hash<T>();
    auto result = generatedHashes.emplace(std::move(hash));
    ASSERT_TRUE(result.second);
}

} // anonymous namespace

TEST(Hash, different_types_have_different_hashes)
{
    assertUniqueHashIsGenerated<SimpleContainer>();
    assertUniqueHashIsGenerated<EncapsulatedStructure>();
    assertUniqueHashIsGenerated<ExtendedStructure>();
    assertUniqueHashIsGenerated<std::map<std::string, std::string>>();
    assertUniqueHashIsGenerated<std::unordered_map<std::string, SimpleContainer>>();
}

//-------------------------------------------------------------------------------------------------

namespace {

struct SameStructure1
{
    int i;
};
NX_REFLECTION_INSTRUMENT(SameStructure1, (i))

struct SameStructure2
{
    int i;
};
NX_REFLECTION_INSTRUMENT(SameStructure2, (i))

} // anonymous namespace

TEST(Hash, different_structs_with_same_fields_have_the_same_hash)
{
    ASSERT_TRUE(hash<SameStructure1>() == hash<SameStructure2>());
}

//-------------------------------------------------------------------------------------------------

namespace {

struct DifferentFieldNames1
{
    int i;
};
NX_REFLECTION_INSTRUMENT(DifferentFieldNames1, (i))

struct DifferentFieldNames2
{
    int j;
};
NX_REFLECTION_INSTRUMENT(DifferentFieldNames2, (j))

} // anonymous namespace

TEST(Hash, same_structs_with_diffenent_field_names_have_different_hashes)
{
    ASSERT_FALSE(hash<DifferentFieldNames1>() == hash<DifferentFieldNames2>());
}

//-------------------------------------------------------------------------------------------------

namespace {

struct DifferentTypes1
{
    int i;
};
NX_REFLECTION_INSTRUMENT(DifferentTypes1, (i))

struct DifferentTypes2
{
    std::string i;
};
NX_REFLECTION_INSTRUMENT(DifferentTypes2, (i))

} // anonymous namespace

TEST(Hash, structs_with_same_field_names_but_different_types_have_different_hashes)
{
    ASSERT_FALSE(hash<DifferentTypes1>() == hash<DifferentTypes2>());
}

//-------------------------------------------------------------------------------------------------

namespace {

struct DifferentFieldsOrder1
{
    int i;
    int j;
};
NX_REFLECTION_INSTRUMENT(DifferentFieldsOrder1, (i)(j))

struct DifferentFieldsOrder2
{
    int j;
    int i;
};
NX_REFLECTION_INSTRUMENT(DifferentFieldsOrder2, (j)(i))

} // anonymous namespace

TEST(Hash, structs_with_same_fields_in_different_order_have_different_hashes)
{
    ASSERT_FALSE(hash<DifferentFieldsOrder1>() == hash<DifferentFieldsOrder2>());
}
