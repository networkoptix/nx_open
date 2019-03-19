#include <gtest/gtest.h>

#include <nx/vms/client/desktop/integrations/internal/data/drawing_items.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop::integrations::internal::roi {
namespace test {

TEST(DrawingItemsTest, DirectionSerialization)
{
    ASSERT_EQ(QnLexical::serialized(Direction::none), "none");
    ASSERT_EQ(QnLexical::deserialized<Direction>("both"), Direction::both);
}

TEST(DrawingItemsTest, BoxSerialization)
{
    Box b;
    b.points = {{0.1, 0.1}, {0.4, 0.4}};
    b.color = Qt::red;
    const QByteArray kBoxRepresentation =
		R"({"color":"#ff0000","points":[[0.1,0.1],[0.4,0.4]]})";

    ASSERT_EQ(QJson::serialized(b), kBoxRepresentation);
    ASSERT_EQ(QJson::deserialized<Box>(kBoxRepresentation), b);
}

TEST(DrawingItemsTest, PolygonSerialization)
{
    Polygon p;
    p.points = {{0.1, 0.2}, {0.5, 0.2}, {0.4, 0.4}, {0.1, 0.4}};
    p.color.setNamedColor("blue");
    const QByteArray kPolygonRepresentation =
		R"({"color":"#0000ff","points":[[0.1,0.2],[0.5,0.2],[0.4,0.4],[0.1,0.4]]})";

    ASSERT_EQ(QJson::serialized(p), kPolygonRepresentation);
    ASSERT_EQ(QJson::deserialized<Polygon>(kPolygonRepresentation), p);
}

TEST(DrawingItemsTest, LineSerialization)
{
    Line l;
    l.points = {{0.1, 0.1}, {0.4, 0.4}};
    l.color = Qt::red;
    l.direction = Direction::both;
    const QByteArray kLineRepresentation =
		R"({"color":"#ff0000","direction":"both","points":[[0.1,0.1],[0.4,0.4]]})";

    ASSERT_EQ(QJson::serialized(l), kLineRepresentation);
    ASSERT_EQ(QJson::deserialized<Line>(kLineRepresentation), l);
}

} // namespace test
} // namespace nx::vms::client::desktop::integrations::internal::roi
