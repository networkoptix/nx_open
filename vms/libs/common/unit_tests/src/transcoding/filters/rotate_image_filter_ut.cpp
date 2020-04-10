#include <gtest/gtest.h>

#include <transcoding/filters/rotate_image_filter.h>

namespace transcoding::filters::test {

TEST(RotateImageFilter, onlyValidAngles)
{
    for (int angle = -370; angle < 370; ++angle)
    {
        QnRotateImageFilter filter(angle);
        ASSERT_TRUE(
            filter.angle() == 0 ||
            filter.angle() == 90 ||
            filter.angle() == 180 ||
            filter.angle() == 270);
    }
}

TEST(RotateImageFilter, values)
{
    ASSERT_EQ(0, QnRotateImageFilter(-370).angle());
    ASSERT_EQ(90, QnRotateImageFilter(100).angle());
    ASSERT_EQ(270, QnRotateImageFilter(-80).angle());
    ASSERT_EQ(270, QnRotateImageFilter(-100).angle());
    ASSERT_EQ(180, QnRotateImageFilter(170).angle());
    ASSERT_EQ(180, QnRotateImageFilter(210).angle());
    ASSERT_EQ(0, QnRotateImageFilter(350).angle());
    ASSERT_EQ(0, QnRotateImageFilter(370).angle());
}

} // namespace transcoding::filters::test
