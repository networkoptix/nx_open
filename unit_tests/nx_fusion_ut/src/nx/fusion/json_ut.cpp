#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

#include <nx/fusion/nx_fusion_test_fixture.h>

namespace {

QByteArray str(const QString& source)
{
    return "\"" + source.toUtf8() + "\"";
}

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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((IntMockData), (json), _Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (BriefMockData)
    (BriefMockDataWithVector)
    (BriefMockDataWithQList),
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
