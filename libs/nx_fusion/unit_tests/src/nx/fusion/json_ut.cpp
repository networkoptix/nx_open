// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/nx_fusion_test_fixture.h>
#include <nx/utils/serialization/flags.h>
#include <nx/utils/serialization/qt_core_types.h>

namespace {

QByteArray str(const QString& source)
{
    return "\"" + source.toUtf8() + "\"";
}

static const nx::Uuid kFirstUuid("0adc055b-3507-4c00-b1c7-d2a789b08d64");
static const nx::Uuid kSecondUuid("251c6b73-674c-4773-a8fc-2577d7a0ecf6");

static const nx::Uuid kTestId("b4a5d7ec-1952-4225-96ed-a08eaf34d97a");
static const QString kHelloWorld("hello world");

struct IntMockData
{
    int a = 661;
    int b = 662;
    int c = 663;

    IntMockData() = default;

    IntMockData(int a, int b, int c): a(a), b(b), c(c) {}

    bool operator==(const IntMockData& other) const
    {
        return a == other.a && b == other.b && c == other.c;
    }

    bool operator!=(const IntMockData& other) const { return !(*this == other); }

    std::string toJsonString() const { return QJson::serialized(*this).toStdString(); }

    static DeprecatedFieldNames* getDeprecatedFieldNames()
    {
        static DeprecatedFieldNames kDeprecatedFieldNames{
            {lit("b"), lit("deprecatedB")},
            {lit("c"), lit("deprecatedC")},
        };
        return &kDeprecatedFieldNames;
    }
};
#define IntMockData_Fields (a)(b)(c)

static bool operator<(const IntMockData& first, const IntMockData& second)
{
    return first.a < second.a && first.b < second.b && first.c < second.c;
};

struct MockDataWithVectorOfSameStruct
{
    std::vector<MockDataWithVectorOfSameStruct> items;

    bool operator==(const MockDataWithVectorOfSameStruct& other) const
    {
        return items == other.items;
    }

    bool operator!=(const MockDataWithVectorOfSameStruct& other) const
    {
        return !(*this == other);
    }
};
#define MockDataWithVectorOfSameStruct_Fields (items)

struct MockDataWithMaps
{
    std::map<IntMockData, int> intItems;
    std::map<MockDataWithMaps, int> nestedItems;

    bool operator==(const MockDataWithMaps& other) const
    {
        return intItems == other.intItems && nestedItems == other.nestedItems;
    }

    bool operator!=(const MockDataWithMaps& other) const { return !(*this == other); }
};
#define MockDataWithMaps_Fields (intItems)(nestedItems)

static bool operator<(const MockDataWithMaps& first, const MockDataWithMaps& second)
{
    return first.intItems < second.intItems;
};

struct MockDataWithStringMaps
{
    std::map<QString, MockDataWithStringMaps> items;

    bool operator==(const MockDataWithStringMaps& other) const
    {
        return items == other.items;
    }

    bool operator!=(const MockDataWithStringMaps& other) const { return !(*this == other); }
};
#define MockDataWithStringMaps_Fields (items)

static bool operator<(const MockDataWithStringMaps& first, const MockDataWithStringMaps& second)
{
    return first.items < second.items;
};

struct BriefMockData
{
    nx::Uuid id;
    QString str;
    BriefMockData() = default;
    BriefMockData(const nx::Uuid& id, const QString& str): id(id), str(str) {}

    bool operator==(const BriefMockData& other) const
    {
        return id == other.id && str == other.str;
    }

    bool operator!=(const BriefMockData& other) const { return !(*this == other); }

};
#define BriefMockData_Fields (id)(str)

struct BriefMockDataWithVector
{
    std::vector<QString> items;

    bool operator==(const BriefMockDataWithVector& other) const
    {
        return items == other.items;
    }

    bool operator!=(const BriefMockDataWithVector& other) const { return !(*this == other); }
};
#define BriefMockDataWithVector_Fields (items)

struct BriefMockDataWithQList
{
    QList<QString> items;

    bool operator==(const BriefMockDataWithQList& other) const
    {
        return items == other.items;
    }

    bool operator!=(const BriefMockDataWithQList& other) const { return !(*this == other); }
};
#define BriefMockDataWithQList_Fields (items)

struct BriefMockDataWithQJsonValue
{
    int pre = 666;
    QJsonValue nullVal;
    bool t = true;
    QJsonValue tVal;
    bool f = false;
    QJsonValue fVal;
    double d = 123.45;
    QJsonValue dVal;
    QString s = kHelloWorld;
    QJsonValue sVal;
    int post = 999;

    bool operator==(const BriefMockDataWithQJsonValue& other) const
    {
        return pre == other.pre
            && nullVal == other.nullVal
            && t == other.t
            && tVal == other.tVal
            && f == other.f
            && fVal == other.fVal
            && d == other.d
            && dVal == other.dVal
            && s == other.s
            && sVal == other.sVal
            && post == other.post;
    }
};
#define BriefMockDataWithQJsonValue_Fields (pre)(nullVal)(t)(tVal)(f)(fVal)(d)(dVal)(s)(sVal)(post)

struct BriefMockWithDefaults
{
    bool boolean = true;
    int integer = 42;
    QString string = "string";
    nx::TestFlag flag = nx::Flag1;
    std::vector<QString> vector = {"v0"};

    bool operator==(const BriefMockWithDefaults& other) const
    {
        return boolean == other.boolean
            && integer == other.integer
            && string == other.string
            && flag == other.flag
            && vector == other.vector;
    }

    bool operator!=(const BriefMockWithDefaults& other) const { return !(*this == other); }
};
#define BriefMockWithDefaults_Fields (boolean)(integer)(string)(flag)(vector)

NX_REFLECTION_ENUM_CLASS(MapKeyEnum,
    unknown,
    test
)

struct EnumKeyMap
{
    std::map<MapKeyEnum, std::string> map;
};
#define EnumKeyMap_Fields (map)

struct MapKey
{
    nx::Uuid uuidField;
    int integerField = 0;
    QString stringField;
};
#define MapKey_Fields (uuidField)(integerField)(stringField)

static bool operator<(const MapKey& first, const MapKey& second)
{
    if (first.uuidField != second.uuidField)
        return first.uuidField < second.uuidField;

    if (first.integerField != second.integerField)
        return first.integerField < second.integerField;

    if (first.stringField != second.stringField)
        return first.stringField < second.stringField;

    return false;
}

static bool operator==(const MapKey& first, const MapKey& second)
{
    return first.uuidField == second.uuidField
        && first.integerField == second.integerField
        && first.stringField == second.stringField;
}

static const std::map<MapKey, IntMockData> kMapWithCustomStructAsKey = {
    {{kFirstUuid, 1, "someString"}, {1, 2, 3}},
    {{kSecondUuid, 2, "somethingElse"}, {4, 5, 6}}
};

QByteArray removeSpaceCharacters(const QByteArray input)
{
    return input.simplified().replace(' ', "");
}

const QByteArray kSerializedMapWithCustomStructAsKey = removeSpaceCharacters(nx::format(R"json(
    {
        "{
            \"integerField\": 1,
            \"stringField\": \"someString\",
            \"uuidField\": \"%1\"
        }": {
            "a":1,
            "b":2,
            "c":3
        },
        "{
            \"integerField\": 2,
            \"stringField\": \"somethingElse\",
            \"uuidField\": \"%2\"
        }": {
            "a":4,
            "b":5,
            "c":6
        }
    })json")
        .args(kFirstUuid, kSecondUuid)
        .toQString()
        .toUtf8());

const QByteArray kMapWithCustomStructAsKeySerializedToArray = removeSpaceCharacters(R"json(
    [
        {
            "key": {
                "integerField": 1,
                "stringField": "someString",
                "uuidField": "{0adc055b-3507-4c00-b1c7-d2a789b08d64}"
            },
            "value": {
                "a":1,
                "b":2,
                "c":3
            }
        },
        {
            "key": {
                "integerField": 2,
                "stringField": "somethingElse",
                "uuidField": "{251c6b73-674c-4773-a8fc-2577d7a0ecf6}"
            },
            "value": {
                "a": 4,
                "b": 5,
                "c": 6
            }
        }
    ])json");

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IntMockData, (json), IntMockData_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BriefMockDataWithQJsonValue, (json), BriefMockDataWithQJsonValue_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BriefMockData, (json), BriefMockData_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    MockDataWithVectorOfSameStruct, (json), MockDataWithVectorOfSameStruct_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MockDataWithMaps, (json), MockDataWithMaps_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MockDataWithStringMaps, (json), MockDataWithStringMaps_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BriefMockDataWithVector, (json), BriefMockDataWithVector_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BriefMockDataWithQList, (json), BriefMockDataWithQList_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BriefMockWithDefaults, (json), BriefMockWithDefaults_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MapKey, (json), MapKey_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EnumKeyMap, (json), EnumKeyMap_Fields, (brief, true))

} // namespace

class QnJsonTextFixture: public QnFusionTestFixture
{
protected:
    template<typename Value>
    void serializationTest(const Value& value, const QByteArray& serialized)
    {
        EXPECT_EQ(QJson::serialized(value), serialized);
        EXPECT_EQ(value, QJson::deserialized<Value>(serialized));
    }
};

TEST_F(QnJsonTextFixture, integralTypes)
{
    ASSERT_EQ("5", QJson::serialized(5));
    ASSERT_EQ(5, QJson::deserialized<int>("5"));
    ASSERT_EQ("-12", QJson::serialized(-12));
    ASSERT_EQ(-12, QJson::deserialized<int>("-12"));

    ASSERT_EQ("\"100000000000\"", QJson::serialized((std::uint64_t) 100000000000ULL));
    ASSERT_EQ(100000000000ULL, QJson::deserialized<std::uint64_t>("\"100000000000\""));
    ASSERT_EQ(100000000000ULL, QJson::deserialized<std::uint64_t>("100000000000"));
}

TEST_F(QnJsonTextFixture, enumKeyedMap)
{
    EnumKeyMap keymap{.map{{MapKeyEnum::test, "test"}}};

    QnJsonContext serializeContext;
    serializeContext.setSerializeMapToObject(true);
    QByteArray serialized;
    QJson::serialize(&serializeContext, keymap, &serialized);

    QnJsonContext deserializeCtx;
    deserializeCtx.setStrictMode(true);
    EnumKeyMap result;
    ASSERT_TRUE(QJson::deserialize(
        &deserializeCtx, QJsonValue(QJsonDocument::fromJson(serialized).object()), &result))
        << "Unable to deserialize a struct with an enum keyed map.";
}

TEST_F(QnJsonTextFixture, chrono_duration)
{
    using namespace std::chrono;

    ASSERT_EQ("\"3\"", QJson::serialized(seconds(3)));
    ASSERT_EQ(seconds(30), QJson::deserialized<seconds>("\"30\""));
    ASSERT_EQ(seconds(30), QJson::deserialized<seconds>("30"));

    ASSERT_EQ("\"5\"", QJson::serialized(milliseconds(5)));
    ASSERT_EQ(milliseconds(50), QJson::deserialized<milliseconds>("\"50\""));
    ASSERT_EQ(milliseconds(50), QJson::deserialized<milliseconds>("50"));

    ASSERT_EQ("\"7\"", QJson::serialized(microseconds(7)));
    ASSERT_EQ(microseconds(70), QJson::deserialized<microseconds>("\"70\""));
    ASSERT_EQ(microseconds(70), QJson::deserialized<microseconds>("70"));

    // Jan 01, 2020, 00:00.
    const microseconds timestamp(1577836800000000LL);
    ASSERT_EQ("\"1577836800000000\"", QJson::serialized(timestamp));
    ASSERT_EQ(timestamp, QJson::deserialized<microseconds>("\"1577836800000000\""));
    ASSERT_EQ(timestamp, QJson::deserialized<microseconds>("1577836800000000"));
}

TEST_F(QnJsonTextFixture, chrono_time_point)
{
    using namespace std::chrono;

    const auto now = floor<milliseconds>(system_clock::now());
    const auto nowMsStr = QByteArray::number(
        (long long) duration_cast<milliseconds>(now.time_since_epoch()).count());
    const auto nowMsStrQuoted = "\"" + nowMsStr + "\"";

    ASSERT_EQ(nowMsStrQuoted, QJson::serialized(now));
    ASSERT_EQ(now, QJson::deserialized<system_clock::time_point>(nowMsStrQuoted));
    ASSERT_EQ(now, QJson::deserialized<system_clock::time_point>(nowMsStr));
}

namespace {

template<typename T> QByteArray serializedAsDouble(T value)
{
    QnJsonContext jsonContext;
    jsonContext.setChronoSerializedAsDouble(true);

    QByteArray result;
    QJson::serialize(&jsonContext, value, &result);
    return result;
}

} // namespace

TEST_F(QnJsonTextFixture, chrono_duration_as_double)
{
    using namespace std::chrono;

    ASSERT_EQ("3", serializedAsDouble(seconds(3)));
    ASSERT_EQ("5", serializedAsDouble(milliseconds(5)));
    ASSERT_EQ("7", serializedAsDouble(microseconds(7)));

    // 05.06.2255 23:47:34.740992
    const microseconds timestamp(1LL << 53);
    ASSERT_EQ("9007199254740992", serializedAsDouble(timestamp));
}

TEST_F(QnJsonTextFixture, chrono_time_point_as_double)
{
    using namespace std::chrono;

    const auto now = floor<milliseconds>(system_clock::now());
    const auto nowMsStr = QByteArray::number(
        (long long) duration_cast<milliseconds>(now.time_since_epoch()).count());

    ASSERT_EQ(nowMsStr, serializedAsDouble(now));
}

TEST_F(QnJsonTextFixture, QtStringTypes)
{
    //ASSERT_EQ(kHelloWorld, QJson::serialized(kHelloWorld.toUtf8())); -- not supported, returns base64
    ASSERT_EQ(str(kHelloWorld), QJson::serialized(kHelloWorld));
}

TEST_F(QnJsonTextFixture, stdStringTypes)
{
    // ASSERT_EQ(kHelloWorld, QJson::serialized("hello world")); -- not supported, return "true"
    ASSERT_EQ(str(kHelloWorld), QJson::serialized(kHelloWorld.toStdString()));
}

TEST_F(QnJsonTextFixture, deserializeSpecialUuids)
{
    nx::Uuid uuid;
    ASSERT_TRUE(QJson::deserialize(QByteArray("null"), &uuid));
    ASSERT_TRUE(uuid.isNull());
    ASSERT_TRUE(QJson::deserialize(QByteArray("\"\""), &uuid));
    ASSERT_TRUE(uuid.isNull());
}

TEST_F(QnJsonTextFixture, enumValue)
{
    const auto value = str("Flag0");
    nx::TestFlag flag = nx::Flag0;
    ASSERT_EQ(value, QJson::serialized(flag));
    ASSERT_EQ(flag, QJson::deserialized<nx::TestFlag>(value));
}

TEST_F(QnJsonTextFixture, enumValueCaseSensitive)
{
    const auto value = str("Flag0");
    nx::TestFlag flag = nx::Flag0;
    ASSERT_EQ(flag, QJson::deserialized<nx::TestFlag>(value.toLower()));
    ASSERT_EQ(flag, QJson::deserialized<nx::TestFlag>(value.toUpper()));
}

TEST_F(QnJsonTextFixture, flagsValue)
{
    const auto value = str("Flag1|Flag2");
    nx::TestFlags flags = nx::Flag1 | nx::Flag2;
    ASSERT_EQ(value, QJson::serialized(flags));
    ASSERT_EQ(flags, QJson::deserialized<nx::TestFlags>(value));
}

TEST_F(QnJsonTextFixture, qtEnums)
{
    const auto value = str("Horizontal|Vertical");
    Qt::Orientations flags = Qt::Horizontal | Qt::Vertical;
    ASSERT_EQ(value, QJson::serialized(flags));
    ASSERT_EQ(flags, QJson::deserialized<Qt::Orientations>(value));
}

TEST_F(QnJsonTextFixture, invalidFlagsLexical)
{
    const auto value = str("Flag7");
    nx::TestFlags flags = nx::Flag0;
    ASSERT_FALSE(QJson::deserialize(value, &flags));
    ASSERT_EQ(nx::Flag0, flags);
}

TEST_F(QnJsonTextFixture, flagsNumeric)
{
    const auto value = str("3");
    nx::TestFlags flags = nx::Flag0;
    ASSERT_TRUE(QJson::deserialize(value, &flags));
    ASSERT_EQ(nx::Flag1|nx::Flag2, flags);
}

TEST_F(QnJsonTextFixture, explicitNullEnumDeserialization)
{
    const auto defaultValue = BriefMockWithDefaults().flag;
    const auto data = QJson::deserialized<BriefMockWithDefaults>(R"json({"flag":null})json");
    EXPECT_EQ(data.flag, defaultValue);
}

TEST_F(QnJsonTextFixture, bitArray)
{
    QBitArray value(17);
    value.setBit(1);
    value.setBit(5);
    value.setBit(6);
    value.setBit(9);
    value.setBit(11);
    value.setBit(16);

    const auto data = QJson::serialized(value);
    ASSERT_EQ(data, str("YgoBAQ==")); //< Base64 is expected.

    QBitArray target;
    ASSERT_TRUE(QJson::deserialize(data, &target));
    ASSERT_EQ(value, target);
}

TEST_F(QnJsonTextFixture, QByteArray)
{
    serializationTest(QByteArray("12345", /*include 0 terminator*/ 6), R"json("MTIzNDUA")json");
}

TEST_F(QnJsonTextFixture, StdArrayI8)
{
    serializationTest(std::array<int8_t, 5>{1, 2, 3, 4, 5}, R"json([1,2,3,4,5])json");
}

TEST_F(QnJsonTextFixture, StdArrayU8)
{
    serializationTest(std::array<uint8_t, 5>{1, 2, 3, 4, 5}, R"json([1,2,3,4,5])json");
}

TEST_F(QnJsonTextFixture, StdArrayInt16)
{
    serializationTest(std::array<int16_t, 5>{1, 2, 3, 4, 5}, R"json([1,2,3,4,5])json");
}

TEST_F(QnJsonTextFixture, StdArrayInt32)
{
    serializationTest(std::array<int32_t, 5>{1, 2, 3, 4, 5}, R"json([1,2,3,4,5])json");
}

TEST_F(QnJsonTextFixture, qJsonValue)
{
    BriefMockDataWithQJsonValue value;
    value.tVal = value.t;
    value.fVal = value.f;
    value.dVal = value.d;
    value.sVal = value.s;

    BriefMockDataWithQJsonValue result = QJson::deserialized<BriefMockDataWithQJsonValue>(
        QJson::serialized(value));

    ASSERT_EQ(value, result);
}

TEST_F(QnJsonTextFixture, deserializeStruct)
{
    const IntMockData data(113, 115, 117);

    const QByteArray jsonStr = R"json(
        {
            "a": 113,
            "b": 115,
            "c": 117
        }
    )json";

    IntMockData deserializedData;
    ASSERT_TRUE(QJson::deserialize(jsonStr, &deserializedData));
    ASSERT_EQ(data, deserializedData);
}

TEST_F(QnJsonTextFixture, deserializeStructWithDeprecatedFields)
{
    const IntMockData data(113, 115, 117);

    const QByteArray jsonStr = R"json(
        {
            "a": 113,
            "deprecatedB": 115,
            "deprecatedC": 117
        }
    )json";

    IntMockData deserializedData;
    ASSERT_TRUE(QJson::deserialize(jsonStr, &deserializedData));
    ASSERT_EQ(data, deserializedData);
}

TEST_F(QnJsonTextFixture, serializeStructBriefId)
{
    BriefMockData data{nx::Uuid(), kHelloWorld};
    const QByteArray jsonStr = QString(R"json({"str":"%1"})json").arg(kHelloWorld).toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<BriefMockData>(jsonStr));
}

TEST_F(QnJsonTextFixture, serializeStructBriefStr)
{
    BriefMockData data{kTestId, QString()};
    const QByteArray jsonStr = QString(R"json({"id":"%1"})json").arg(kTestId.toString()).toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<BriefMockData>(jsonStr));
}

TEST_F(QnJsonTextFixture, serializeStructBriefBoth)
{
    BriefMockData data;
    const QByteArray jsonStr = QString(R"json({})json").toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<BriefMockData>(jsonStr));
}

TEST_F(QnJsonTextFixture, serializeStructListBrief)
{
    std::vector<BriefMockData> data;
    data.emplace_back(nx::Uuid(), kHelloWorld);
    data.emplace_back(kTestId, QString());
    data.emplace_back(nx::Uuid(), QString());
    const QByteArray jsonStr = QString(R"json([{"str":"%1"},{"id":"%2"},{}])json")
        .arg(kHelloWorld)
        .arg(kTestId.toString())
        .toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<std::vector<BriefMockData>>(jsonStr));
}

TEST_F(QnJsonTextFixture, serializeStdMapStringToStructBrief)
{
    std::map<QString, BriefMockData> data;
    data["a"] = {{}, kHelloWorld};
    data["b"] = {kTestId, {}};
    data["c"] = {};
    const QByteArray jsonStr = QString(R"json({"a":{"str":"%1"},"b":{"id":"%2"},"c":{}})json")
        .arg(kHelloWorld)
        .arg(kTestId.toString())
        .toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<decltype(data)>(jsonStr));
}

TEST_F(QnJsonTextFixture, serializeStdMapIntToStructBrief)
{
    std::map<int, BriefMockData> data;
    data[1] = {{}, kHelloWorld};
    data[2] = {kTestId, {}};
    const QByteArray jsonStr = QString(
        R"json([{"key":1,"value":{"str":"%1"}},{"key":2,"value":{"id":"%2"}}])json")
            .arg(kHelloWorld)
            .arg(kTestId.toString())
            .toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<decltype(data)>(jsonStr));
}

TEST_F(QnJsonTextFixture, serializeVectorBrief)
{
    BriefMockDataWithVector data;
    data.items.push_back(kHelloWorld);
    data.items.push_back(QString());
    data.items.push_back(kHelloWorld);
    const QByteArray jsonStr = QString(R"json({"items":["%1","","%1"]})json")
        .arg(kHelloWorld)
        .toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<BriefMockDataWithVector>(jsonStr));
}

TEST_F(QnJsonTextFixture, serializeEmptyVectorBrief)
{
    BriefMockDataWithVector data;
    const QByteArray jsonStr = QString(R"json({})json").toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<BriefMockDataWithVector>(jsonStr));
}

TEST_F(QnJsonTextFixture, serializeStructWithStdVectorSameStruct){
    MockDataWithVectorOfSameStruct data;
    data.items = std::vector<MockDataWithVectorOfSameStruct>();
    const QByteArray jsonStr = QString(R"json({"items":[]})json").toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<decltype(data)>(jsonStr));
}

TEST_F(QnJsonTextFixture, serializeStructWithStdMaps)
{
    MockDataWithMaps data;
    data.intItems = std::map<IntMockData, int>();
    data.nestedItems = std::map<MockDataWithMaps, int>();
    const QByteArray jsonStr = QString(R"json({"intItems":[],"nestedItems":[]})json").toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<decltype(data)>(jsonStr));
}

TEST_F(QnJsonTextFixture, serializeStructWithStdMapsWithDefaultSerialization)
{
    QnJsonContext context;
    context.setOptionalDefaultSerialization(true);
    MockDataWithMaps data;
    data.intItems = std::map<IntMockData, int>();
    data.nestedItems = std::map<MockDataWithMaps, int>();
    const QByteArray jsonStr = /*suppress newline*/ 1 + (const char*)
R"json(
{
    "intItems": [
        {
            "key": {
                "a": 661,
                "b": 662,
                "c": 663
            },
            "value": 0
        }
    ],
    "nestedItems": [
        {
            "key": {
                "intItems": [
                    {
                        "key": {
                            "a": 661,
                            "b": 662,
                            "c": 663
                        },
                        "value": 0
                    }
                ],
                "nestedItems": []
            },
            "value": 0
        }
    ]
})json";

    QByteArray actualJson;
    QJson::serialize(&context, data, &actualJson);

    ASSERT_EQ(jsonStr, nx::utils::formatJsonString(actualJson));
}

TEST_F(QnJsonTextFixture, serializeStructWithStringMapsWithDefaultSerialization)
{
    QnJsonContext context;
    context.setOptionalDefaultSerialization(true);
    MockDataWithStringMaps data;
    data.items = std::map<QString, MockDataWithStringMaps>();
    const QByteArray jsonStr = /*suppress newline*/ 1 + (const char*)
R"json(
{
    "items": {
        "": {
            "items": {}
        }
    }
})json";

    QByteArray actualJson;
    QJson::serialize(&context, data, &actualJson);

    ASSERT_EQ(jsonStr, nx::utils::formatJsonString(actualJson));
}

TEST_F(QnJsonTextFixture, serializeQListBrief)
{
    BriefMockDataWithQList data;
    data.items.push_back(kHelloWorld);
    data.items.push_back(QString());
    data.items.push_back(kHelloWorld);
    const QByteArray jsonStr = QString(R"json({"items":["%1","","%1"]})json")
        .arg(kHelloWorld)
        .toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<BriefMockDataWithQList>(jsonStr));
}

TEST_F(QnJsonTextFixture, serializeEmptyQListBrief)
{
    BriefMockDataWithQList data;
    const QByteArray jsonStr = QString(R"json({})json").toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<BriefMockDataWithQList>(jsonStr));
}

TEST_F(QnJsonTextFixture, briefWithDefaultsSimetry)
{
    BriefMockWithDefaults data;
    const QByteArray jsonStr =
        R"json({"boolean":true,"flag":"Flag1","integer":42,"string":"string","vector":["v0"]})json";
    EXPECT_EQ(jsonStr, QJson::serialized(data));
    EXPECT_EQ(data, QJson::deserialized<BriefMockWithDefaults>(jsonStr));
}

TEST_F(QnJsonTextFixture, DISABLED_briefWithDefaultsNullsSimetry)
{
    BriefMockWithDefaults data{false, 0, "", nx::Flag0, {}};
    const QByteArray jsonStr = R"json({"boolean":false,"flag":"Flag0","integer":0})json";
    EXPECT_EQ(jsonStr, QJson::serialized(data));

    // Test fails here due ot string and vector defaults.
    EXPECT_EQ(data, QJson::deserialized<BriefMockWithDefaults>(jsonStr));
}

TEST_F(QnJsonTextFixture, briefWithDefaultsNotNullsSimetry)
{
    BriefMockWithDefaults data{true, 777, "not-null", nx::Flag2, {"v8"}};
    const QByteArray jsonStr =
        R"json({"boolean":true,"flag":"Flag2","integer":777,"string":"not-null","vector":["v8"]})json";
    EXPECT_EQ(jsonStr, QJson::serialized(data));
    EXPECT_EQ(data, QJson::deserialized<BriefMockWithDefaults>(jsonStr));
}

TEST_F(QnJsonTextFixture, serializeMapToObjects)
{
    static const std::map<nx::Uuid, int> data = {
        {kFirstUuid, 1},
        {kSecondUuid, 2}
    };

    const QByteArray expectedJson = nx::format(R"json({"%1":1,"%2":2})json")
        .args(kFirstUuid, kSecondUuid).toQString().toUtf8();

    QnJsonContext context;
    context.setSerializeMapToObject(true);

    QByteArray actualJson;
    QJson::serialize(&context, data, &actualJson);

    ASSERT_EQ(expectedJson, actualJson);
}

TEST_F(QnJsonTextFixture, serializeMapToObjectUsingCustomStructAsKey)
{
    QnJsonContext context;
    context.setSerializeMapToObject(true);

    QByteArray actualJson;
    QJson::serialize(&context, kMapWithCustomStructAsKey, &actualJson);

    ASSERT_EQ(kSerializedMapWithCustomStructAsKey, actualJson);
}

TEST_F(QnJsonTextFixture, deserializeMapWithCustomStructAsKey)
{
    std::map<MapKey, IntMockData> deserialized;

    ASSERT_TRUE(QJson::deserialize(kSerializedMapWithCustomStructAsKey, &deserialized));
    ASSERT_EQ(kMapWithCustomStructAsKey, deserialized);
}

TEST_F(QnJsonTextFixture, deserializeMapFromArray)
{
    std::map<MapKey, IntMockData> deserialized;
    ASSERT_TRUE(QJson::deserialize(kMapWithCustomStructAsKeySerializedToArray, &deserialized));
    ASSERT_EQ(kMapWithCustomStructAsKey, deserialized);
}

TEST_F(QnJsonTextFixture, deserializeMapWithUuidAsKey)
{
    static const std::map<nx::Uuid, int> kExpectedMap = {{kFirstUuid, 1}, {kSecondUuid, 2}};
    static const QByteArray kSerializedMap =
        nx::format(R"json({"%1":1,"%2":2})json").args(kFirstUuid, kSecondUuid).toUtf8();

    std::map<nx::Uuid, int> actualMap;
    ASSERT_TRUE(QJson::deserialize(kSerializedMap, &actualMap));
    ASSERT_EQ(actualMap, kExpectedMap);
}

TEST_F(QnJsonTextFixture, deserializeMapWithIntegerAsKey)
{
    static const std::map<int, int> kExpectedMap = {{1, 1}, {2, 2}};
    static const QByteArray kSerializedMap(R"json({"1":1,"2":2})json");

    std::map<int, int> actualMap;
    ASSERT_TRUE(QJson::deserialize(kSerializedMap, &actualMap));
    ASSERT_EQ(actualMap, kExpectedMap);
}

TEST_F(QnJsonTextFixture, deserializeMapWithIncorrectJsonAsKeyFails)
{
    static const QByteArray kSerializedMap(R"json({"{a}":1,"{2}":2})json");

    std::map<MapKey, int> result;
    ASSERT_FALSE(QJson::deserialize(kSerializedMap, &result));
}

//-------------------------------------------------------------------------------------------------

namespace {

struct TestDataWithOptionalField
{
    std::optional<BriefMockData> optionalField;
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TestDataWithOptionalField, (json), (optionalField))

struct OptionalJsonValue
{
    std::optional<QJsonValue> field;
};
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(OptionalJsonValue, (json), (field))

} // namespace

class QnJsonBoostOptionalField:
    public QnJsonTextFixture
{
};

TEST_F(QnJsonBoostOptionalField, serialize_optional_field_present)
{
    TestDataWithOptionalField test;
    test.optionalField = BriefMockData{nx::Uuid(), "test"};

    ASSERT_EQ(
        "{\"optionalField\":{\"str\":\"test\"}}",
        QJson::serialized(test));
}

TEST_F(QnJsonBoostOptionalField, serialize_optional_field_not_present)
{
    ASSERT_EQ("{}", QJson::serialized(TestDataWithOptionalField()));
}

TEST_F(QnJsonBoostOptionalField, deserialize_optional_field_present)
{
    const auto json = "{\"optionalField\":{\"str\":\"test\"}}";
    const auto deserializedValue = QJson::deserialized<TestDataWithOptionalField>(json);

    ASSERT_TRUE(static_cast<bool>(deserializedValue.optionalField));
    ASSERT_EQ("test", deserializedValue.optionalField->str);
}

TEST_F(QnJsonBoostOptionalField, deserialize_optional_field_not_present)
{
    const auto json = "{}";
    const auto deserializedValue = QJson::deserialized<TestDataWithOptionalField>(json);

    ASSERT_FALSE(static_cast<bool>(deserializedValue.optionalField));
}

TEST_F(QnJsonBoostOptionalField, serialize_optional_json_value)
{
    ASSERT_EQ("{}", QJson::serialized(OptionalJsonValue{}).toStdString());
    ASSERT_EQ(
        "{\"field\":null}", QJson::serialized(OptionalJsonValue{QJsonValue()}).toStdString());
}

TEST_F(QnJsonBoostOptionalField, serialize_optional)
{
    ASSERT_EQ("0", QJson::serialized(std::optional<int>(0)));
    ASSERT_EQ("", QJson::serialized(std::optional<int>()));
}

TEST_F(QnJsonBoostOptionalField, deserialize_optional)
{
    ASSERT_EQ(QJson::deserialized<std::optional<int>>(" 1 ").value_or(0), 1);

    bool ok = false;
    ASSERT_FALSE(QJson::deserialized("  ", std::optional<int>(), &ok));
    ASSERT_TRUE(ok);
}

//-------------------------------------------------------------------------------------------------

namespace {

NX_REFLECTION_ENUM_CLASS(NxReflectFlag, flag1 = 0x1, flag2 = 0x2, flag3 = 0x4)
Q_DECLARE_FLAGS(NxReflectFlags, NxReflectFlag)

struct NxReflectFlagsStruct
{
    NxReflectFlag flag = NxReflectFlag::flag2;
    NxReflectFlags flags = NxReflectFlag::flag2;

    bool operator==(const NxReflectFlagsStruct& other) const
    {
        return flag == other.flag && flags == other.flags;
    }
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NxReflectFlagsStruct, (json), (flag)(flags))

} // namespace

TEST(QnJsonReflectEnums, serializeInstrumentedEnums)
{
    const auto serialized = QJson::serialized(NxReflectFlagsStruct{});
    const auto expected = R"json({"flag":"flag2","flags":"flag2"})json";

    EXPECT_EQ(serialized, expected);
}

TEST(QnJsonReflectEnums, deserializeInstrumentedEnums)
{
    const auto serialized = R"json({"flag":"flag1","flags":"flag2|flag3"})json";
    const auto expected = NxReflectFlagsStruct{
        NxReflectFlag::flag1, NxReflectFlags(NxReflectFlag::flag2) | NxReflectFlag::flag3};
    const auto deserialized = QJson::deserialized<NxReflectFlagsStruct>(serialized);

    EXPECT_EQ(deserialized, expected);
}
