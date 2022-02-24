// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <transcoding/filters/scale_image_filter.h>
#include <utils/media/frame_info.h>

#include <nx/utils/nx_utils_ini.h>
#include <nx/build_info.h>

namespace {

CLVideoDecoderOutputPtr generateFrame(const QSize& size)
{
    CLVideoDecoderOutputPtr frame(new CLVideoDecoderOutput());
    frame->reallocate(size, AV_PIX_FMT_YUV420P);
    return frame;
}

} // namespace

class ScaleFilterTest: public ::testing::Test
{
public:
    ScaleFilterTest(const CLVideoDecoderOutputPtr &frame = CLVideoDecoderOutputPtr()) :
        m_frame(frame),
        m_iniTweaks(new nx::kit::IniConfig::Tweaks())
    {
        // Windows specifics: DebugBreak is called on NX_ASSERT even if crashes are disabled, and
        // CI catches it. On other OS we can check the backup behavior when assert fails.
        if (nx::build_info::isWindows())
            m_iniTweaks->set(&nx::utils::ini().assertCrash, true);
    }

    void thenScalingSuccessful(const QSize& size)
    {
        QnScaleImageFilter filter(size);
        CLVideoDecoderOutputPtr frame = filter.updateImage(m_frame);
        ASSERT_EQ(frame->size(), size);
    }

    void thenScalingFailed(const QSize& size)
    {
        QnScaleImageFilter filter(size);
        if (nx::utils::ini().assertCrash)
        {
            ASSERT_DEATH(filter.updateImage(m_frame), "Error while scaling frame .*");
        }
        else
        {
            CLVideoDecoderOutputPtr frame = filter.updateImage(m_frame);
            ASSERT_EQ(frame->size(), m_frame->size());
        }
    }

    void givenFrame(const QSize& size)
    {
        m_frame = generateFrame(size);
    }

private:
    CLVideoDecoderOutputPtr m_frame;
    std::unique_ptr<nx::kit::IniConfig::Tweaks> m_iniTweaks;
};

class BatchScaleFilterTest: public ScaleFilterTest
{
public:
    BatchScaleFilterTest():
        ScaleFilterTest(),
        m_step(0),
        m_shouldFail(false)
    {}

    void testValidSizes()
    {
        setSizesAndStep(QSize(50, 50), QSize(1000, 1000), 100);
        setShouldFail(false);
        testAllSizes();
    }

private:
    void setSizesAndStep(const QSize& startSize, const QSize& endSize, int step)
    {
        m_startSize = startSize;
        m_endSize = endSize;
        m_step = step;
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
        givenFrame(frameSize);

        forEachSize([this] (const QSize& size)
            {
                if (m_shouldFail)
                    thenScalingFailed(size);
                else
                    thenScalingSuccessful(size);
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
    QSize m_startSize;
    QSize m_endSize;
    int m_step;
    bool m_shouldFail;
};

TEST_F(ScaleFilterTest, ffmpegIssues)
{
    // In some cases scaling fails deep inside of ffmpeg without any sensible reason.
    givenFrame({5, 605});
    thenScalingSuccessful({105, 5});
    thenScalingFailed({205, 5});
}

TEST_F(ScaleFilterTest, invalidFrameSize)
{
    givenFrame({0, 50});
    thenScalingFailed({50, 50});

    givenFrame({50, 0});
    thenScalingFailed({50, 50});
}

TEST_F(ScaleFilterTest, invalidFilterSize)
{
    givenFrame({50, 50});
    thenScalingFailed({0, 50});
    thenScalingFailed({50, 0});
}

TEST_F(BatchScaleFilterTest, validSizes)
{
    testValidSizes();
}
