#include <nx/utils/log/to_string.h>

#include <gtest/gtest.h>

#include <QtCore>
#include <QtNetwork/QTcpSocket>

#include <nx/utils/uuid.h>

namespace nx {
namespace utils {
namespace test {

template<typename T>
void assertToString(const T& value, const char* string)
{
    EXPECT_EQ(QString::fromUtf8(string), toString(value));
}

TEST(ToString, BuildIn)
{
    assertToString((int) 1, "1");
    assertToString((uint) 77777, "77777");

    assertToString((float) 2.5, "2.5");
    assertToString((double) 12.34, "12.34");

    assertToString("hello", "hello");
    assertToString("hello world", "hello world");

    assertToString((const char*) "hello", "hello");
    assertToString((const char*) "hello world", "hello world");
}

TEST(ToString, Std)
{
    assertToString(std::string("string"), "string");
    assertToString(std::string("some long string"), "some long string");

    assertToString(std::chrono::milliseconds(100), "100ms");
    assertToString(std::chrono::milliseconds(3000), "3s");

    assertToString(std::chrono::seconds(10), "10s");
    assertToString(std::chrono::seconds(60 * 60 * 2), "2h");

    assertToString(std::chrono::minutes(5), "5m");
    assertToString(std::chrono::minutes(65), "65m");

    assertToString(std::chrono::hours(2), "2h");
    assertToString(std::chrono::hours(200), "200h");
}

TEST(ToString, Qt)
{
    assertToString(QString::fromUtf8("hello"), "hello");
    assertToString(QString::fromUtf8("hello world today"), "hello world today");

    assertToString(QByteArray("hello"), "hello");
    assertToString(QByteArray("hello world today"), "hello world today");

    assertToString(QUrl(QLatin1String("http://abc.xyz:8080/path")), "http://abc.xyz:8080/path");
    assertToString(QUrl(QLatin1String("http://xxx-yyy.w/path?param=3")), "http://xxx-yyy.w/path?param=3");
    assertToString(QUrl(QLatin1String("http://login:password@abx-xyz.x")), "http://login@abx-xyz.x");

    assertToString(QPoint(123, 456), "QPoint(123,456)");
    assertToString(QPointF(2.5, 3.6), "QPointF(2.5,3.6)");

    assertToString(QSize(123, 456), "QSize(123, 456)");
    assertToString(QSizeF(2.5, 3.6), "QSizeF(2.5, 3.6)");

    assertToString(QRect(12, 34, 56, 78), "QRect(12,34 56x78)");
    assertToString(QRect(QPoint(1, 2), QSize(3, 4)), "QRect(1,2 3x4)");

    assertToString(QRectF(1.2, 3.4, 5.6, 7.8), "QRectF(1.2,3.4 5.6x7.8)");
    assertToString(QRectF(QPointF(1, 2), QSizeF(3, 4)), "QRectF(1,2 3x4)");

    assertToString(Qt::Edge::TopEdge, "Qt::Edge(TopEdge)");
    assertToString(Qt::Edge::LeftEdge, "Qt::Edge(LeftEdge)");

    assertToString(QAbstractSocket::AddressInUseError, "QAbstractSocket::AddressInUseError");
    assertToString(QAbstractSocket::NetworkError, "QAbstractSocket::NetworkError");

    // TODO: Add some more QT types
}

struct CustomStringableType
{
    int i = 0;
    CustomStringableType(int i = 0): i(i) {}
    QString toString() const { return QLatin1String("CustomStringableType") + QString::number(i); }
};

struct CustomDebugableType
{
    int i = 0;
    CustomDebugableType(int i = 0): i(i) {}
};
static QDebug operator<<(QDebug debug, const CustomDebugableType& d)
{
    return debug << "CustomDebugableType" << d.i;
}

TEST(ToString, Custom)
{
    assertToString(CustomStringableType(), "CustomStringableType0");
    assertToString(CustomStringableType(555), "CustomStringableType555");

    assertToString(CustomDebugableType(), "CustomDebugableType0");
    assertToString(CustomDebugableType(77777), "CustomDebugableType77777");
}

TEST(ToString, NxTypes)
{
    const auto uuid = QnUuid::createUuid();
    assertToString(uuid, uuid.toStdString().c_str());

    // TODO: Add some more NX types
}

template<typename T>
void assertToStringPattern(const T& value, const char* pattern)
{
    const auto valueString = toString(value);
    const QRegExp regExp(QString::fromUtf8(pattern), Qt::CaseSensitive, QRegExp::Wildcard);

    EXPECT_TRUE(regExp.exactMatch(valueString))
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
    QString idForToStringFromPtr() const { return toString(i); }
};

TEST(ToString, Pointers)
{
    std::string data("data");
    assertToStringPattern((const void*) data.c_str(), "void(0x*)");

    assertPtrToString<std::string>("std::*string*(0x*)");
    assertPtrToString<std::vector<int>>("std::*vector*<*>(0x*)");
    assertPtrToString<CustomStringableType>("nx::utils::test::CustomStringableType(0x*)");
    assertPtrToString<CustomDebugableType>("nx::utils::test::CustomDebugableType(0x*)");
    assertPtrToString<CustomDebugableType>("nx::utils::test::CustomDebugableType(0x*)");
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
    assertToString(boost::make_optional<int>(777), "777");
    assertToString(boost::make_optional<CustomStringableType>(5), "CustomStringableType5");
    assertToString(boost::make_optional<int>(false, 1), "none");

    assertToString(std::make_pair(1, "hello"), "( 1: hello )");
    assertToString(std::make_pair(2.5, CustomDebugableType()), "( 2.5: CustomDebugableType0 )");

    assertContainerToString(std::vector<int>{1, 2, 3}, "{ 1, 2, 3 }");
    assertContainerToString(std::vector<std::string>{"1st", "2nd", "3rd"}, "{ 1st, 2nd, 3rd }");

    std::map<std::string, int> map = {{"size", 5}, {"weight", 6}, {"OK", 1}};
    assertContainerToString(map, "{ ( OK: 1 ), ( size: 5 ), ( weight: 6 ) }");
}

} // namespace test
} // namespace utils
} // namespace nx
