#include <gtest/gtest.h>

#include <QtGui/QGuiApplication>
#include <QtGui/QCursor>

#include <nx/vms/client/desktop/ui/common/cursor_override.h>

namespace nx::vms::client::desktop {
namespace test {

namespace {

char* dummyName = "test";
int dummyArgC = 1;
char** dummyArgV = &dummyName;

} // namespace

class CursorOverrideTest: public testing::Test
{
public:
    CursorOverrideTest(): app(dummyArgC, dummyArgV) {}

    static constexpr Qt::CursorShape noCursor = Qt::CursorShape(-1);

    Qt::CursorShape currentCursor() const
    {
        return QGuiApplication::overrideCursor()
            ? QGuiApplication::overrideCursor()->shape()
            : noCursor;
    }

private:
    const QGuiApplication app;
};

TEST_F(CursorOverrideTest, lifo)
{
    QScopedPointer<CursorOverrideAttached> cursor1(new CursorOverrideAttached());
    QScopedPointer<CursorOverrideAttached> cursor2(new CursorOverrideAttached());
    QScopedPointer<CursorOverrideAttached> cursor3(new CursorOverrideAttached());

    cursor1->setShape(Qt::CrossCursor);
    cursor2->setShape(Qt::WaitCursor);
    cursor3->setShape(Qt::BusyCursor);

    ASSERT_EQ(currentCursor(), noCursor);

    cursor1->setActive(true);
    ASSERT_TRUE(cursor1->active());
    ASSERT_TRUE(cursor1->effectivelyActive());
    ASSERT_EQ(cursor1->shape(), Qt::CrossCursor);
    ASSERT_EQ(currentCursor(), Qt::CrossCursor);

    cursor2->setActive(true);
    ASSERT_TRUE(cursor2->active());
    ASSERT_TRUE(cursor2->effectivelyActive());
    ASSERT_FALSE(cursor1->effectivelyActive());
    ASSERT_EQ(cursor2->shape(), Qt::WaitCursor);
    ASSERT_EQ(currentCursor(), Qt::WaitCursor);

    cursor3->setActive(true);
    ASSERT_TRUE(cursor3->active());
    ASSERT_TRUE(cursor3->effectivelyActive());
    ASSERT_FALSE(cursor2->effectivelyActive());
    ASSERT_EQ(cursor3->shape(), Qt::BusyCursor);
    ASSERT_EQ(currentCursor(), Qt::BusyCursor);

    cursor3->setActive(false);
    ASSERT_FALSE(cursor3->active());
    ASSERT_FALSE(cursor3->effectivelyActive());
    ASSERT_TRUE(cursor2->effectivelyActive());
    ASSERT_EQ(currentCursor(), Qt::WaitCursor);

    cursor2.reset();
    ASSERT_EQ(currentCursor(), Qt::CrossCursor);

    cursor1->setActive(false);
    ASSERT_FALSE(cursor1->active());
    ASSERT_FALSE(cursor1->effectivelyActive());
    ASSERT_EQ(currentCursor(), noCursor);
}

TEST_F(CursorOverrideTest, fifo)
{
    QScopedPointer<CursorOverrideAttached> cursor1(new CursorOverrideAttached());
    QScopedPointer<CursorOverrideAttached> cursor2(new CursorOverrideAttached());
    QScopedPointer<CursorOverrideAttached> cursor3(new CursorOverrideAttached());

    cursor1->setShape(Qt::CrossCursor);
    cursor2->setShape(Qt::WaitCursor);
    cursor3->setShape(Qt::BusyCursor);

    ASSERT_EQ(currentCursor(), noCursor);

    ASSERT_EQ(currentCursor(), noCursor);

    cursor1->setActive(true);
    ASSERT_TRUE(cursor1->active());
    ASSERT_TRUE(cursor1->effectivelyActive());
    ASSERT_EQ(cursor1->shape(), Qt::CrossCursor);
    ASSERT_EQ(currentCursor(), Qt::CrossCursor);

    cursor2->setActive(true);
    ASSERT_TRUE(cursor2->active());
    ASSERT_TRUE(cursor2->effectivelyActive());
    ASSERT_FALSE(cursor1->effectivelyActive());
    ASSERT_EQ(cursor2->shape(), Qt::WaitCursor);
    ASSERT_EQ(currentCursor(), Qt::WaitCursor);

    cursor3->setActive(true);
    ASSERT_TRUE(cursor3->active());
    ASSERT_TRUE(cursor3->effectivelyActive());
    ASSERT_FALSE(cursor2->effectivelyActive());
    ASSERT_EQ(cursor3->shape(), Qt::BusyCursor);
    ASSERT_EQ(currentCursor(), Qt::BusyCursor);

    cursor1->setActive(false);
    ASSERT_FALSE(cursor1->active());
    ASSERT_TRUE(cursor3->effectivelyActive());
    ASSERT_EQ(currentCursor(), Qt::BusyCursor);

    cursor2.reset();
    ASSERT_TRUE(cursor3->effectivelyActive());
    ASSERT_EQ(currentCursor(), Qt::BusyCursor);

    cursor3->setActive(false);
    ASSERT_FALSE(cursor3->active());
    ASSERT_FALSE(cursor3->effectivelyActive());
    ASSERT_EQ(currentCursor(), noCursor);
}

TEST_F(CursorOverrideTest, mixed)
{
    QScopedPointer<CursorOverrideAttached> cursor1(new CursorOverrideAttached());
    QScopedPointer<CursorOverrideAttached> cursor2(new CursorOverrideAttached());
    QScopedPointer<CursorOverrideAttached> cursor3(new CursorOverrideAttached());

    cursor1->setShape(Qt::CrossCursor);
    cursor2->setShape(Qt::WaitCursor);
    cursor3->setShape(Qt::BusyCursor);

    ASSERT_EQ(currentCursor(), noCursor);

    ASSERT_EQ(currentCursor(), noCursor);

    cursor1->setActive(true);
    ASSERT_TRUE(cursor1->active());
    ASSERT_TRUE(cursor1->effectivelyActive());
    ASSERT_EQ(cursor1->shape(), Qt::CrossCursor);
    ASSERT_EQ(currentCursor(), Qt::CrossCursor);

    cursor2->setActive(true);
    ASSERT_TRUE(cursor2->active());
    ASSERT_TRUE(cursor2->effectivelyActive());
    ASSERT_FALSE(cursor1->effectivelyActive());
    ASSERT_EQ(cursor2->shape(), Qt::WaitCursor);
    ASSERT_EQ(currentCursor(), Qt::WaitCursor);

    cursor3->setActive(true);
    ASSERT_TRUE(cursor3->active());
    ASSERT_TRUE(cursor3->effectivelyActive());
    ASSERT_FALSE(cursor2->effectivelyActive());
    ASSERT_EQ(cursor3->shape(), Qt::BusyCursor);
    ASSERT_EQ(currentCursor(), Qt::BusyCursor);

    cursor2.reset();
    ASSERT_EQ(currentCursor(), Qt::BusyCursor);
    ASSERT_TRUE(cursor3->effectivelyActive());

    cursor3->setActive(false);
    ASSERT_FALSE(cursor3->active());
    ASSERT_FALSE(cursor3->effectivelyActive());
    ASSERT_TRUE(cursor1->effectivelyActive());
    ASSERT_EQ(currentCursor(), Qt::CrossCursor);

    cursor1->setActive(false);
    ASSERT_FALSE(cursor1->active());
    ASSERT_FALSE(cursor1->effectivelyActive());
    ASSERT_EQ(currentCursor(), noCursor);
}

} // namespace test
} // namespace nx::vms::client::desktop
