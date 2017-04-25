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

struct LazyMockData
{
    QnUuid id;
    QString str;
    LazyMockData() = default;
    LazyMockData(const QnUuid& id, const QString& str): id(id), str(str) {}

    bool operator==(const LazyMockData& other) const
    {
        return id == other.id && str == other.str;
    }

    bool operator!=(const LazyMockData& other) const { return !(*this == other); }

};
#define LazyMockData_Fields (id)(str)

struct LazyMockDataWithVector
{
    std::vector<QString> items;

    bool operator==(const LazyMockDataWithVector& other) const
    {
        return items == other.items;
    }

    bool operator!=(const LazyMockDataWithVector& other) const { return !(*this == other); }
};
#define LazyMockDataWithVector_Fields (items)

struct LazyMockDataWithQList
{
    QList<QString> items;

    bool operator==(const LazyMockDataWithQList& other) const
    {
        return items == other.items;
    }

    bool operator!=(const LazyMockDataWithQList& other) const { return !(*this == other); }
};
#define LazyMockDataWithQList_Fields (items)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((IntMockData), (json), _Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (LazyMockData)
    (LazyMockDataWithVector)
    (LazyMockDataWithQList),
    (json), _Fields, (lazy, true))

} // namespace

TEST_F(QnFusionTestFixture, integralTypes)
{
    ASSERT_EQ("5", QJson::serialized(5));
    ASSERT_EQ(5, QJson::deserialized<int>("5"));
    ASSERT_EQ("-12", QJson::serialized(-12));
    ASSERT_EQ(-12, QJson::deserialized<int>("-12"));
}

TEST_F(QnFusionTestFixture, QtStringTypes)
{
    //ASSERT_EQ(kHelloWorld, QJson::serialized(kHelloWorld.toUtf8())); -- not supported, returns base64
    ASSERT_EQ(str(kHelloWorld), QJson::serialized(kHelloWorld));
}

TEST_F(QnFusionTestFixture, stdStringTypes)
{
    // ASSERT_EQ(kHelloWorld, QJson::serialized("hello world")); -- not supported, return "true"
    ASSERT_EQ(str(kHelloWorld), QJson::serialized(kHelloWorld.toStdString()));
}

TEST_F(QnFusionTestFixture, enumValue)
{
    const auto value = str("Flag0");
    nx::TestFlag flag = nx::Flag0;
    ASSERT_EQ(value, QJson::serialized(flag));
    ASSERT_EQ(flag, QJson::deserialized<nx::TestFlag>(value));
}

TEST_F(QnFusionTestFixture, flagsValue)
{
    const auto value = str("Flag1|Flag2");
    nx::TestFlags flags = nx::Flag1 | nx::Flag2;
    ASSERT_EQ(value, QJson::serialized(flags));
    ASSERT_EQ(flags, QJson::deserialized<nx::TestFlags>(value));
}

TEST_F(QnFusionTestFixture, invalidFlagsLexical)
{
    const auto value = str("Flag7");
    nx::TestFlags flags = nx::Flag0;
    ASSERT_FALSE(QJson::deserialize(value, &flags));
    ASSERT_EQ(nx::Flag0, flags);
}

TEST_F(QnFusionTestFixture, flagsNumeric)
{
    const auto value = str("3");
    nx::TestFlags flags = nx::Flag0;
    ASSERT_TRUE(QJson::deserialize(value, &flags));
    ASSERT_EQ(nx::Flag1|nx::Flag2, flags);
}

TEST_F(QnFusionTestFixture, deserializeStruct)
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

TEST_F(QnFusionTestFixture, deserializeStructWithDeprecatedFields)
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

TEST_F(QnFusionTestFixture, serializeStructLazyId)
{
    LazyMockData data{QnUuid(), kHelloWorld};
    const QByteArray jsonStr = QString(R"json({"str":"%1"})json").arg(kHelloWorld).toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<LazyMockData>(jsonStr));
}

TEST_F(QnFusionTestFixture, serializeStructLazyStr)
{
    LazyMockData data{kTestId, QString()};
    const QByteArray jsonStr = QString(R"json({"id":"%1"})json").arg(kTestId.toString()).toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<LazyMockData>(jsonStr));
}

TEST_F(QnFusionTestFixture, serializeStructLazyBoth)
{
    LazyMockData data;
    const QByteArray jsonStr = QString(R"json({})json").toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<LazyMockData>(jsonStr));
}

TEST_F(QnFusionTestFixture, serializeStructListLazy)
{
    std::vector<LazyMockData> data;
    data.emplace_back(QnUuid(), kHelloWorld);
    data.emplace_back(kTestId, QString());
    data.emplace_back(QnUuid(), QString());
    const QByteArray jsonStr = QString(R"json([{"str":"%1"},{"id":"%2"},{}])json")
        .arg(kHelloWorld)
        .arg(kTestId.toString())
        .toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<std::vector<LazyMockData>>(jsonStr));
}

TEST_F(QnFusionTestFixture, serializeVectorLazy)
{
    LazyMockDataWithVector data;
    data.items.push_back(kHelloWorld);
    data.items.push_back(QString());
    data.items.push_back(kHelloWorld);
    const QByteArray jsonStr = QString(R"json({"items":["%1","","%1"]})json")
        .arg(kHelloWorld)
        .toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<LazyMockDataWithVector>(jsonStr));
}

TEST_F(QnFusionTestFixture, serializeEmptyVectorLazy)
{
    LazyMockDataWithVector data;
    const QByteArray jsonStr = QString(R"json({})json").toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<LazyMockDataWithVector>(jsonStr));
}

TEST_F(QnFusionTestFixture, serializeQListLazy)
{
    LazyMockDataWithQList data;
    data.items.push_back(kHelloWorld);
    data.items.push_back(QString());
    data.items.push_back(kHelloWorld);
    const QByteArray jsonStr = QString(R"json({"items":["%1","","%1"]})json")
        .arg(kHelloWorld)
        .toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<LazyMockDataWithQList>(jsonStr));
}

TEST_F(QnFusionTestFixture, serializeEmptyQListLazy)
{
    LazyMockDataWithQList data;
    const QByteArray jsonStr = QString(R"json({})json").toUtf8();
    ASSERT_EQ(jsonStr, QJson::serialized(data));
    ASSERT_EQ(data, QJson::deserialized<LazyMockDataWithQList>(jsonStr));
}