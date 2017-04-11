#include <gtest/gtest.h>

#include <nx/utils/app_info.h>
#include <transcoding/filters/scale_image_filter.h>
#include <utils/media/frame_info.h>

class FilterTest
{
public:
    FilterTest(const CLVideoDecoderOutputPtr &frame = CLVideoDecoderOutputPtr()) :
        m_filter(QSize()),
        m_frame(frame)
    {}

    void setFrame(const CLVideoDecoderOutputPtr& frame)
    {
        m_frame = frame;
    }

    void testSuccessWithSize(const QSize& size)
    {
        doTest(size, false);
    }

    void testCertainFailWithSize(const QSize& size)
    {
        doTest(size, true);
    }

private:
    void doTest(const QSize& size, bool shouldFail)
    {
        ASSERT_TRUE((bool)m_frame);

        m_filter.setOutputImageSize(size);
        CLVideoDecoderOutputPtr frameCopy = m_filter.updateImage(m_frame);

        if (shouldFail)
            ASSERT_FALSE((bool)frameCopy);
        else
        {
            if (frameCopy)
            {
                ASSERT_EQ(frameCopy->size().width(), size.width());
                ASSERT_EQ(frameCopy->size().height(), size.height());
            }
        }
    }

private:
    QnScaleImageFilter m_filter;
    CLVideoDecoderOutputPtr m_frame;
};

class ScaleFilterTest: public ::testing::Test
{
public:
    ScaleFilterTest() :
        m_step(0),
        m_shouldFail(false)
    {}

    void testValidSizes()
    {
        setSizesAndStep(QSize(5, 5), QSize(1000, 1000), 100);
        setShouldFail(false);
        testAllSizes();
    }

    void testInvalidFilterSize()
    {
        setSizesAndStep(QSize(0,5), QSize(0, 1000), 100);
        setShouldFail(true);
        testWithExactFrame(QSize(5,5));
        testWithExactFrame(QSize(105,50));
        testWithExactFrame(QSize(700,890));
        testWithExactFrame(QSize(0, 0));

        setSizesAndStep(QSize(5,0), QSize(1000, 0), 100);
        setShouldFail(true);
        testWithExactFrame(QSize(5,5));
        testWithExactFrame(QSize(105,50));
        testWithExactFrame(QSize(700,890));
        testWithExactFrame(QSize(0, 0));
    }

    void testInvalidFrameSize()
    {
        setSizesAndStep(QSize(5,5), QSize(1000, 1000), 100);
        setShouldFail(true);
        testWithExactFrame(QSize(0,5));
        testWithExactFrame(QSize(0,0));
        testWithExactFrame(QSize(105,0));
    }

private:

    void setSizesAndStep(const QSize& startSize, const QSize& endSize, int step)
    {
        m_startSize = startSize;
        m_endSize = endSize;
        m_step = step;

        if (nx::utils::AppInfo::isArm()) //< Reduce load to make test run faster.
            m_step *= 2;
    }

    void setShouldFail(bool shouldFail)
    {
        m_shouldFail = shouldFail;
    }

    void testAllSizes()
    {
        ASSERT_TRUE(m_step != 0);

        forEachSize([this] (const QSize& size) { return testWithExactFrame(size); });
    }

    void testWithExactFrame(const QSize& frameSize)
    {
        m_filterTest.setFrame(generateFrame(frameSize));

        forEachSize([this] (const QSize& size)
            {
                if (m_shouldFail)
                    m_filterTest.testCertainFailWithSize(size);
                else
                    m_filterTest.testSuccessWithSize(size);
            });
    }

    template <typename F>
    void forEachSize(F f)
    {
        for (int i = m_startSize.width(); i <= m_endSize.width(); i += m_step)
            for (int j = m_startSize.height(); j <= m_endSize.height(); j += m_step)
                f(QSize(i, j));
    }

private:
    CLVideoDecoderOutputPtr generateFrame(const QSize& size)
    {
        CLVideoDecoderOutputPtr frame(new CLVideoDecoderOutput());
        frame->reallocate(size, AV_PIX_FMT_YUV420P);

        return frame;
    }

private:
    QSize m_startSize;
    QSize m_endSize;
    int m_step;
    FilterTest m_filterTest;
    bool m_shouldFail;
};

TEST_F(ScaleFilterTest, ImageSizes)
{
    testValidSizes();
    testInvalidFilterSize();
    testInvalidFrameSize();
}
