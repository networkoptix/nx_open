#include <gtest/gtest.h>

#include <nx/analytics/db/test_support/custom_gtest_printers.h>
#include <analytics/db/analytics_db_utils.h>
#include <motion/motion_detection.h>

namespace nx::analytics::db::test {

TEST(AnalyticsDbUtils_translateToSearchGrid, result_rect_is_always_non_empty)
{
    auto translated = translateToSearchGrid(
        QRectF(0.886326, 0.347431, 3.91545e-05, 0.0728283));
    ASSERT_GT(translated.width() * translated.height(), 0);
    //ASSERT_EQ(2, translated.width());

    translated = translateToSearchGrid(
        QRectF(0.886326, 0.0, 3.91545e-05, 0.0));
    ASSERT_GT(translated.width() * translated.height(), 0);
    //ASSERT_EQ(2, translated.width());
    ASSERT_EQ(1, translated.height());

    translated = translateToSearchGrid(
        QRectF(0.886326, 0.347431, 0.0, 0.0));
    ASSERT_GT(translated.width() * translated.height(), 0);
    ASSERT_EQ(1, translated.width());
    ASSERT_EQ(1, translated.height());
}

TEST(AnalyticsDbUtils_translateToSearchGrid, result_rect_is_correct)
{
    ASSERT_EQ(
        QRect(kTrackSearchGrid.width() - 1, 0, 1, kTrackSearchGrid.height()),
        translateToSearchGrid(QRectF(0.99999, 0.0, 0.00001, 1)));

    ASSERT_EQ(
        kTrackSearchGrid,
        translateToSearchGrid(QRectF(0.0, 0.0, 1.0, 1.0)));
}

} // namespace nx::analytics::db::test
