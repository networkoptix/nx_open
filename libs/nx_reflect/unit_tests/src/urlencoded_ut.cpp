// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <array>

#include <QUuid>

#include <nx/reflect/instrument.h>
#include <nx/reflect/urlencoded.h>

namespace nx::reflect::urlencoded::test {

struct FlatData
{
    int intV = 0;
    bool boolV = false;
    double doubleV = 0.;
    std::string strV;
    std::optional<int> optIntX;
    std::chrono::seconds secondsV = std::chrono::seconds::zero();

    void compare(const FlatData& r)
    {
        ASSERT_EQ(
            std::tie(intV, boolV, strV, optIntX, secondsV),
            std::tie(r.intV, r.boolV, r.strV, r.optIntX, r.secondsV));
        ASSERT_EQ(doubleV, r.doubleV);
    }

    static FlatData data()
    {
        return FlatData{
            10, false, -50.5, "str with {} ~ special symbols", 50, std::chrono::seconds(10)};
    }
};

struct FlatDataWithResultStr: public FlatData
{
    using ParentT = FlatData;
    static constexpr std::string_view encoded =
        "intV=10&boolV=false&doubleV=-50.500000&strV=str%20with%20%7B%7D%20~%20special%20symbols&optIntX=50&secondsV=10";
};

NX_REFLECTION_INSTRUMENT(FlatData, (intV)(boolV)(doubleV)(strV)(optIntX)(secondsV));

struct ArrayData
{
    std::vector<int> intVec;
    std::set<std::string> strSet;
    std::map<std::string, std::string> strStrMap;
    void compare(const ArrayData& r)
    {
        ASSERT_EQ(intVec, r.intVec);
        ASSERT_EQ(strSet, r.strSet);
        ASSERT_EQ(strStrMap, r.strStrMap);
    }

    static ArrayData data()
    {
        return ArrayData{
            {1, 0, 3, -4, 5}, {"s1", "s2", "s3"}, {{"a`", "_x"}, {"b", "y"}, {"c", "z"}}};
    }
};

struct ArrayDataWithResultStr: public ArrayData
{
    using ParentT = ArrayData;
    static constexpr std::string_view encoded =
        "intVec=[1,0,3,-4,5]&strSet=[s1,s2,s3]&strStrMap={a%60=_x&b=y&c=z}";
};

NX_REFLECTION_INSTRUMENT(ArrayData, (intVec)(strSet)(strStrMap));

struct ComplexArrayData
{
    std::vector<std::vector<int>> intVecVec;
    std::map<std::string, std::vector<int>> strVecIntMap;
    std::vector<std::map<std::string, std::string>> MapStrStrVec;
    void compare(const ComplexArrayData& r)
    {
        ASSERT_EQ(intVecVec, r.intVecVec);
        ASSERT_EQ(strVecIntMap, r.strVecIntMap);
        ASSERT_EQ(MapStrStrVec, r.MapStrStrVec);
    }

    static ComplexArrayData data()
    {
        return ComplexArrayData{
            {{0, 1, 2}, {10, 11}, {0}},
            {{"s1", {1, 2}}, {"s2", {0}}},
            {{{"a", "1"}, {"b", "2"}}, {{"c", "3"}}}};
    }
};

struct ComplexArrayDataWithResultStr: public ComplexArrayData
{
    using ParentT = ComplexArrayData;
    static constexpr std::string_view encoded =
        "intVecVec=[[0,1,2],[10,11],[0]]&strVecIntMap={s1=[1,2]&s2=[0]}&MapStrStrVec=[{a=1&b=2},{c=3}]";
};

NX_REFLECTION_INSTRUMENT(ComplexArrayData, (intVecVec)(strVecIntMap)(MapStrStrVec));

enum class EnumClass
{
    val0 = 0,
    val1,
    val2
};

NX_REFLECTION_INSTRUMENT_ENUM(EnumClass, val0, val1, val2)

struct ComplexObjData
{
    int intV = 0;
    FlatData testData1;
    ArrayData testData2;
    EnumClass enumV = EnumClass::val0;

    void compare(const ComplexObjData& r)
    {
        ASSERT_EQ(intV, r.intV);
        ASSERT_EQ(enumV, r.enumV);
        testData1.compare(r.testData1);
        testData2.compare(r.testData2);
    }

    static ComplexObjData data()
    {
        return ComplexObjData{
            42, FlatData{1, true, 0.5, "aa", 3, std::chrono::seconds(10)}, ArrayData{{1, 2}, {"s1", "s2"}, {{"a", "x"}}}, EnumClass::val1};
    }
};

struct ComplexObjDataWithResultStr: public ComplexObjData
{
    using ParentT = ComplexObjData;
    static constexpr std::string_view encoded =
        "intV=42&testData1={intV=1&boolV=true&doubleV=0.500000&strV=aa&optIntX=3&secondsV=10}&testData2={intVec=[1,2]&strSet=[s1,s2]&strStrMap={a=x}}&enumV=val1";
};

NX_REFLECTION_INSTRUMENT(ComplexObjData, (intV)(testData1)(testData2)(enumV));

template<typename T>
class Urlencoded: public ::testing::Test
{
};

TYPED_TEST_SUITE_P(Urlencoded);

TYPED_TEST_P(Urlencoded, encode)
{
    using baseType = typename TypeParam::ParentT;
    auto encodedStr = urlencoded::serialize(baseType::data());
    ASSERT_EQ(encodedStr, TypeParam::encoded);
}

TYPED_TEST_P(Urlencoded, decode)
{
    using baseType = typename TypeParam::ParentT;
    auto str = TypeParam::encoded;
    baseType val;
    auto decoded = urlencoded::deserialize(str, &val);
    ASSERT_TRUE(decoded);
    val.compare(TypeParam::ParentT::data());
}

TYPED_TEST_P(Urlencoded, encodeDecode)
{
    using baseType = typename TypeParam::ParentT;
    auto encodedStr = urlencoded::serialize(baseType::data());
    ASSERT_EQ(encodedStr, TypeParam::encoded);
    baseType val;
    auto decoded = urlencoded::deserialize(encodedStr, &val);
    ASSERT_TRUE(decoded);
    val.compare(TypeParam::ParentT::data());
}

TYPED_TEST_P(Urlencoded, decoding_empty_string_produces_default_object)
{
    using DataType = typename TypeParam::ParentT;

    auto [decoded, result] = urlencoded::deserialize<DataType>(std::string());
    ASSERT_TRUE(result);
    decoded.compare(DataType());
}

REGISTER_TYPED_TEST_SUITE_P(Urlencoded,
    encode,
    decode,
    encodeDecode,
    decoding_empty_string_produces_default_object);

typedef ::testing::Types<
    FlatDataWithResultStr,
    ArrayDataWithResultStr,
    ComplexObjDataWithResultStr,
    ComplexArrayDataWithResultStr>
    TestingTypes;
INSTANTIATE_TYPED_TEST_SUITE_P(UrlencodedEncode, Urlencoded, TestingTypes);


struct TestStringDeserialization
{
    std::string str = "cloud-test.hdw.mx cloud-test1.hdw.mx";
};

NX_REFLECTION_INSTRUMENT(TestStringDeserialization, (str));

TEST(UrlencodedStr, BugCB339)
{
    TestStringDeserialization val;
    auto decoded = urlencoded::deserialize("str=cloud-test.hdw.mx%20cloud-test1.hdw.mx", &val);
    ASSERT_TRUE(decoded);
    ASSERT_EQ("cloud-test.hdw.mx cloud-test1.hdw.mx", val.str);
}

//-------------------------------------------------------------------------------------------------

struct Stringizable
{
    std::string text;

    std::string toString() const { return text; }

    static Stringizable fromString(const std::string_view& str)
    {
        return Stringizable{std::string(str)};
    }
};

struct Foo1
{
    Stringizable str;
};

NX_REFLECTION_INSTRUMENT(Foo1, (str))

TEST(UrlencodedStr, toString_is_used_to_serialize_a_value)
{
    const auto actual = urlencoded::serialize(Foo1{.str={.text="Hello_world"}});
    ASSERT_EQ("str=Hello_world", actual);
}

TEST(UrlencodedStr, fromString_is_used_to_deserialize_a_value)
{
    const auto expected = Foo1{.str={.text="Hello_world"}};
    const auto [actual, result] = urlencoded::deserialize<Foo1>("str=Hello_world");
    ASSERT_TRUE(result);
    ASSERT_EQ(expected.str.text, actual.str.text);
}

//-------------------------------------------------------------------------------------------------

struct Foo2
{
    bool bar = false;
    std::optional<bool> optBar;

    bool operator==(const Foo2&) const = default;
};

NX_REFLECTION_INSTRUMENT(Foo2, (bar)(optBar))

TEST(Urlencoded, bool_flag_presence_is_handled_as_true)
{
    const auto [val, result] = urlencoded::deserialize<Foo2>("bar&optBar");
    ASSERT_TRUE(result);

    Foo2 expected{true, true};
    ASSERT_EQ(expected, val);
}

} // namespace nx::reflect::urlencoded::test
