// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/utils/log/to_string.h>

#include <gtest/gtest.h>

#include <bitset>
#include <optional>

#include <QtCore>
#include <QtNetwork/QTcpSocket>

#include <nx/utils/uuid.h>
#include <nx/utils/log/log.h>
#include <nx/utils/serialization/flags.h>

namespace nx {
namespace utils {
namespace test {

template<typename T>
void assertToString(const T& value, const char* string)
{
    EXPECT_EQ(QString::fromUtf8(string), nx::toString(value));
}

TEST(ToString, BuiltIn)
{
    assertToString((int) 1, "1");
    assertToString((uint) 77777, "77777");

    assertToString((float) 2.5, "2.5");
    assertToString((double) 12.34, "12.34");

    assertToString("hello", "hello");
    assertToString("hello world", "hello world");

    assertToString((const char*) "hello", "hello");
    assertToString((const char*) "hello world", "hello world");

    assertToString((const char*) nullptr, "");
}

TEST(ToString, Std)
{
    assertToString(std::string("string"), "string");
    assertToString(std::string("some long string"), "some long string");
    assertToString(std::string_view("Hello, world"), "Hello, world");

    assertToString(std::chrono::nanoseconds(100), "100ns");
    assertToString(std::chrono::nanoseconds(3000), "3us");

    assertToString(std::chrono::microseconds(100), "100us");
    assertToString(std::chrono::microseconds(3000), "3ms");

    assertToString(std::chrono::milliseconds(100), "100ms");
    assertToString(std::chrono::milliseconds(3000), "3s");

    assertToString(std::chrono::seconds(10), "10s");
    assertToString(std::chrono::seconds(60 * 60 * 2), "2h");

    assertToString(std::chrono::minutes(5), "5m");
    assertToString(std::chrono::minutes(65), "65m");

    assertToString(std::chrono::hours(2), "2h");
    assertToString(std::chrono::hours(200), "200h");

    assertToString(std::bitset<8>(), "0b00000000");
    assertToString(std::bitset<8>(0xFF), "0b11111111");
}

TEST(ToString, Qt)
{
    assertToString(QString::fromUtf8("hello"), "hello");
    assertToString(QString::fromUtf8("hello world today"), "hello world today");

    assertToString(QByteArray("hello"), "hello");
    assertToString(QByteArray("hello world today"), "hello world today");

    QString tmpQString("hello world today");
    assertToString(QStringView(tmpQString), "hello world today");

    assertToString(QLatin1String("hello world today"), "hello world today");

    assertToString(QUrl(QLatin1String("http://abc.xyz:8080/path")), "http://abc.xyz:8080/path");
    assertToString(QUrl(QLatin1String("http://xxx-yyy.w/path?param=3")), "http://xxx-yyy.w/path?param=3");
    if (nx::log::showPasswords())
        assertToString(QUrl(QLatin1String("http://login:password@abx-xyz.x")), "http://login:password@abx-xyz.x");
    else
        assertToString(QUrl(QLatin1String("http://login:password@abx-xyz.x")), "http://login@abx-xyz.x");

    assertToString(QPoint(123, 456), "QPoint(123,456)");
    assertToString(QPointF(2.5, 3.6), "QPointF(2.5,3.6)");

    assertToString(QSize(123, 456), "QSize(123, 456)");
    assertToString(QSizeF(2.5, 3.6), "QSizeF(2.5, 3.6)");

    assertToString(QRect(12, 34, 56, 78), "QRect(12,34 56x78)");
    assertToString(QRect(QPoint(1, 2), QSize(3, 4)), "QRect(1,2 3x4)");

    assertToString(QRectF(1.2, 3.4, 5.6, 7.8), "QRectF(1.2,3.4 5.6x7.8)");
    assertToString(QRectF(QPointF(1, 2), QSizeF(3, 4)), "QRectF(1,2 3x4)");

    assertToString(Qt::Edge::TopEdge, "Qt::TopEdge");
    assertToString(Qt::Edge::LeftEdge, "Qt::LeftEdge");

    assertToString(QAbstractSocket::AddressInUseError, "QAbstractSocket::AddressInUseError");
    assertToString(QAbstractSocket::NetworkError, "QAbstractSocket::NetworkError");

    assertToString(QJsonValue(), "null");
    assertToString(QJsonValue(true), "true");
    assertToString(QJsonValue(false), "false");

    assertToString(QJsonValue(7), "7");
    assertToString(QJsonValue(7.7), "7.7");

    assertToString(QJsonValue("id"), "id");
    assertToString(QJsonValue("Hello world"), "Hello world");

    assertToString(QJsonValue({false, 1, "2"}), R"json([false,1,"2"])json");
    assertToString(QJsonArray({false, 1, "2"}), R"json([false,1,"2"])json");

    assertToString(QJsonValue({{"a", 1}, {"b", 2}}), R"json({"a":1,"b":2})json");
    assertToString(QJsonObject({{"a", 1}, {"b", 2}}), R"json({"a":1,"b":2})json");

    // TODO: Add some more QT types
}

struct CustomStringableType
{
    int i = 0;
    CustomStringableType(int i = 0): i(i) {}
    QString toString() const { return QLatin1String("CustomStringableType") + QString::number(i); }
};

struct CustomDebuggableType
{
    int i = 0;
    CustomDebuggableType(int i = 0): i(i) {}
};
static QDebug operator<<(QDebug debug, const CustomDebuggableType& d)
{
    return debug << "CustomDebuggableType" << d.i;
}

TEST(ToString, Custom)
{
    assertToString(CustomStringableType(), "CustomStringableType0");
    assertToString(CustomStringableType(555), "CustomStringableType555");

    assertToString(CustomDebuggableType(), "CustomDebuggableType0");
    assertToString(CustomDebuggableType(77777), "CustomDebuggableType77777");
}

TEST(ToString, NxTypes)
{
    const auto uuid = nx::Uuid::createUuid();
    assertToString(uuid, uuid.toStdString().c_str());

    // TODO: Add some more Nx types.
}

template<typename T>
void assertToStringPattern(const T& value, const char* pattern)
{
    const auto valueString = nx::toString(value);
    const auto wildcard = QRegularExpression::wildcardToRegularExpression(QString::fromUtf8(pattern));
    const QRegularExpression regExp(wildcard);

    EXPECT_TRUE(regExp.match(valueString).hasMatch())
        << "<" << valueString.toStdString() << "> does not match pattern <" << pattern << ">";
}

template<typename T>
void assertPtrToString(const char* pattern)
{
    T object;
    assertToStringPattern(&object, pattern);

    const std::unique_ptr<T> unique(new T);
    assertToStringPattern(unique, pattern);

    const std::shared_ptr<T> shared(new T);
    assertToStringPattern(shared, pattern);
}

struct CustomBase { virtual ~CustomBase() = default; };
struct CustomInheritor: CustomBase {};

struct CustomInt
{
    int i = 0;
    CustomInt(int i = 0): i(i) {}
    QString idForToStringFromPtr() const { return nx::toString(i); }
};

TEST(ToString, Pointers)
{
    std::string data("data");
    assertToStringPattern((const void*) data.c_str(), "void(0x*)");

    assertPtrToString<std::string>("std::*string*(0x*)");
    assertPtrToString<std::vector<int>>("std::*vector*<*>(0x*)");
    assertPtrToString<CustomStringableType>("nx::utils::test::CustomStringableType(0x*)");
    assertPtrToString<CustomDebuggableType>("nx::utils::test::CustomDebuggableType(0x*)");
    assertPtrToString<CustomDebuggableType>("nx::utils::test::CustomDebuggableType(0x*)");
    assertPtrToString<std::optional<CustomDebuggableType>>(
        "std*::optional<nx::utils::test::CustomDebuggableType>(0x*)");
    assertPtrToString<QSize>("QSize(0x*)");

    std::unique_ptr<CustomBase> base(new CustomBase);
    std::unique_ptr<CustomBase> inheritorBase(new CustomInheritor);
    std::unique_ptr<CustomInheritor> inheritor(new CustomInheritor);
    assertToStringPattern(base, "nx::utils::test::CustomBase(0x*)");
    assertToStringPattern(inheritorBase, "nx::utils::test::CustomInheritor(0x*)");
    assertToStringPattern(inheritor, "nx::utils::test::CustomInheritor(0x*)");

    std::unique_ptr<CustomInt> int0(new CustomInt);
    std::unique_ptr<CustomInt> int777(new CustomInt{777});
    assertToStringPattern(int0, "nx::utils::test::CustomInt(0x*, 0)");
    assertToStringPattern(int777, "nx::utils::test::CustomInt(0x*, 777)");
}

template<typename T>
void assertContainerToString(const T& value, const char* string)
{
    assertToString(containerString(value), string);
}

TEST(ToString, Containers)
{
    assertToString(std::make_optional<int>(777), "777");
    assertToString(std::make_optional<CustomStringableType>(5), "CustomStringableType5");
    assertToString(std::optional<int>(std::nullopt), "none");

    assertToString(std::make_pair(1, "hello"), "( 1: hello )");
    assertToString(std::make_pair(2.5, CustomDebuggableType()), "( 2.5: CustomDebuggableType0 )");

    assertToString(std::vector<int>{1, 2, 3}, "{ 1, 2, 3 }");

    assertContainerToString(std::vector<int>{1, 2, 3}, "{ 1, 2, 3 }");
    assertContainerToString(std::vector<std::string>{"1st", "2nd", "3rd"}, "{ 1st, 2nd, 3rd }");

    std::map<std::string, int> map = {{"size", 5}, {"weight", 6}, {"OK", 1}};
    assertContainerToString(map, "{ ( OK: 1 ), ( size: 5 ), ( weight: 6 ) }");
}

NX_REFLECTION_ENUM_CLASS(ReflectedEnum, a = 0x1, b = 0x2, c = 0x4)
Q_DECLARE_FLAGS(ReflectedEnumFlags, ReflectedEnum)

TEST(ToString, ReflectedEnum)
{
    assertToString(ReflectedEnum::c, "c");
    assertToString(ReflectedEnumFlags(ReflectedEnum::b) | ReflectedEnum::c, "b|c");
}

TEST(ToString, StdVariant)
{
    std::variant<int, std::string> v1 = 42;
    assertToString(v1, "42");

    std::variant<int, std::string> v2 = "hello";
    assertToString(v2, "hello");

    std::variant<int, std::variant<int, std::string>> v3 = v2;
    assertToString(v3, "hello");
}

} // namespace test
} // namespace utils
} // namespace nx
