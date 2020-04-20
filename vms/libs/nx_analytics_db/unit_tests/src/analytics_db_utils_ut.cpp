#include <gtest/gtest.h>

#include <analytics/db/analytics_db_utils.h>

namespace nx::analytics::db::test {

TEST(AnalyticsDbUtils_translateToSearchGrid, result_rect_is_always_non_empty)
{
    const auto translated = translateToSearchGrid(
        QRectF(0.886326, 0.347431, 3.91545e-05, 0.0728283));
    ASSERT_GT(translated.width() * translated.height(), 0);
}

} // namespace nx::analytics::db::test
