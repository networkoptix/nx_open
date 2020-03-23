#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/qt_enums.h>

#include <nx/fusion/nx_fusion_test_fixture.h>

namespace {

QByteArray str(const QString& source)
{
    return "\"" + source.toUtf8() + "\"";
}

static const QnUuid kFirstUuid("0adc055b-3507-4c00-b1c7-d2a789b08d64");
static const QnUuid kSecondUuid("251c6b73-674c-4773-a8fc-2577d7a0ecf6");

static const QnUuid kTestId("b4a5d7ec-1952-4225-96ed-a08eaf34d97a");
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

struct BriefMockData
{
    QnUuid id;
    QString str;
    BriefMockData() = default;
    BriefMockData(const QnUuid& id, const QString& str): id(id), str(str) {}

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

struct MapKey
{
    QnUuid uuidField;
    int integerField = 0;
    QString stringField;
};
#define MapKey_Fields (uuidField)(integerField)(stringField)

static bool operator<(const MapKey& first, const MapKey& second)
{
    if (first.uuidField != second.uuidField)
        return first.uuidField < second.uuidField;

    if (second.integerField != second.integerField)
        return second.integerField < second.integerField;

    if (second.stringField != second.stringField)
        return second.stringField < second.stringField;

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

const QByteArray kSerializedMapWithCustomStructAsKey = lm(
    "{"
        "\"{"
            "\\\"integerField\\\":1,"
            "\\\"stringField\\\":\\\"someString\\\","
            "\\\"uuidField\\\":\\\"%1\\\""
        "}\":{\"a\":1,\"b\":2,\"c\":3},"
        "\"{"
            "\\\"integerField\\\":2,"
            "\\\"stringField\\\":\\\"somethingElse\\\","
            "\\\"uuidField\\\":\\\"%2\\\""
        "}\":{\"a\":4,\"b\":5,\"c\":6}"
    "}")
        .args(kFirstUuid, kSecondUuid)
        .toQString()
        .toUtf8();

const QByteArray kMapWithCustomStructAsKeySerializedToArray =
    "["
        "{"
            "\"key\":{"
                "\"integerField\":1,"
                "\"stringField\":\"someString\","
                "\"uuidField\":\"{0adc055b-3507-4c00-b1c7-d2a789b08d64}\""
            "},"
            "\"value\":{\"a\":1,\"b\":2,\"c\":3}"
        "},"
        "{"
            "\"key\":{"
                "\"integerField\":2,"
                "\"stringField\":\"somethingElse\","
                "\"uuidField\":\"{251c6b73-674c-4773-a8fc-2577d7a0ecf6}\""
            "},"
            "\"value\":{\"a\":4,\"b\":5,\"c\":6}"
        "}"
    "]";

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((IntMockData), (json), _Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (BriefMockData)
    (BriefMockDataWithVector)
    (BriefMockDataWithQList)
    (MapKey),
    (json), _Fields, (brief, true))

} // namespace

class QnJsonTextFixture: public QnFusionTestFixture
{
};

TEST_F(QnJsonTextFixture, integralTypes)
{
    ASSERT_EQ("5", QJson::serialized(5));
    ASSERT_EQ(5, QJson::deserialized<int>("5"));
    ASSERT_EQ("-12", QJson::serialized(-12));
    ASSERT_EQ(-12, QJson::deserialized<int>("-12"));
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
    BriefMockData data{QnUuid(), kHelloWorld};
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
    data.emplace_back(QnUuid(), kHelloWorld);
    data.emplace_back(kTestId, QString());
    data.emplace_back(QnUuid(), QString());
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

TEST_F(QnJsonTextFixture, serializeMapToObjects)
{
    static const std::map<QnUuid, int> data = {
        {kFirstUuid, 1},
        {kSecondUuid, 2}
    };

    const QByteArray expectedJson = lm("{\"%1\":1,\"%2\":2}")
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

//-------------------------------------------------------------------------------------------------

namespace {

struct TestDataWithOptionalField
{
    boost::optional<BriefMockData> optionalField;
};

#define TestDataWithOptionalField_Fields (optionalField)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (TestDataWithOptionalField), (json), _Fields)

} // namespace

class QnJsonBoostOptionalField:
    public QnJsonTextFixture
{
};

TEST_F(QnJsonBoostOptionalField, serialize_optional_field_present)
{
    TestDataWithOptionalField test;
    test.optionalField = BriefMockData{QnUuid(), "test"};

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
