#include <gtest/gtest.h>

#include <nx/client/core/utils/geometry.h>
#include <core/resource/layout_resource.h>

#include <nx/client/desktop/resource_properties/layout/redux/layout_settings_dialog_state.h>
#include <nx/client/desktop/resource_properties/layout/redux/layout_settings_dialog_state_reducer.h>

#include <ui/style/globals.h>

namespace nx {
namespace client {
namespace desktop {

class LayoutSettingsDialogStateReducerTest: public testing::Test
{
public:
    using State = LayoutSettingsDialogState;
    using Reducer = LayoutSettingsDialogStateReducer;

protected:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override
    {
        m_globals.reset(new QnGlobals());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() override
    {
        m_globals.reset();
    }

    QImage makeImage(int width, int height)
    {
        QImage img(width, height, QImage::Format::Format_Mono);
        return img;
    }

    QImage makeImage(float aspectRatio)
    {
        constexpr int kSampleHeight = 1000;
        return makeImage(kSampleHeight * aspectRatio, kSampleHeight);
    }

    State setImage(State state, QImage image)
    {
        state.background.preview = image;
        state.background.croppedPreview = image;
        state.background.status = State::BackgroundImageStatus::loaded;
        return state;
    }

    State makeImageState(float aspectRatio)
    {
        State state;
        state = Reducer::clearBackgroundImage(std::move(state));
        state = setImage(std::move(state), makeImage(aspectRatio));
        state.cellAspectRatio = 1.0;
        return state;
    }

private:
    QScopedPointer<QnGlobals> m_globals;
};

TEST_F(LayoutSettingsDialogStateReducerTest, defaultSize)
{
    const int kMinWidth = qnGlobals->layoutBackgroundMinSize().width();
    const int kMinHeight = qnGlobals->layoutBackgroundMinSize().height();

    State s;
    s = Reducer::clearBackgroundImage(std::move(s));
    ASSERT_EQ(s.background.width.value, kMinWidth);
    ASSERT_EQ(s.background.height.value, kMinHeight);
}

// Checking setHeight and setWidth are keeping target aspectRatio of kSampleAspectRatio.
TEST_F(LayoutSettingsDialogStateReducerTest, keepAspectRatioOnSizeChange)
{
    constexpr int kSampleAspectRatio = 4;

    State s = makeImageState(kSampleAspectRatio);
    s.background.keepImageAspectRatio = true;
    s.background.width.setRange(0, std::numeric_limits<int>::max());
    s.background.height.setRange(0, std::numeric_limits<int>::max());

    constexpr int kSampleWidth = 40;
    s = Reducer::setBackgroundImageWidth(std::move(s), kSampleWidth);
    ASSERT_EQ(s.background.height.value, kSampleWidth / kSampleAspectRatio);

    constexpr int kSampleHeight = 12;
    s = Reducer::setBackgroundImageHeight(std::move(s), kSampleHeight);
    ASSERT_EQ(s.background.width.value, kSampleHeight * kSampleAspectRatio);
}

TEST_F(LayoutSettingsDialogStateReducerTest, checkLimitsOnKeepArChange)
{
    constexpr int kSampleAspectRatio = 4;
    const int kMinWidth = qnGlobals->layoutBackgroundMinSize().width();
    const int kMaxWidth = qnGlobals->layoutBackgroundMaxSize().width();
    const int kMinHeight = qnGlobals->layoutBackgroundMinSize().height();
    const int kMaxHeight = qnGlobals->layoutBackgroundMaxSize().height();

    State s = makeImageState(kSampleAspectRatio);
    s.background.keepImageAspectRatio = true;
    s.background.cropToMonitorAspectRatio = false;

    s = Reducer::setKeepImageAspectRatio(std::move(s), true);
    ASSERT_EQ(s.background.width.min, kMinWidth * kSampleAspectRatio);
    ASSERT_EQ(s.background.width.max, kMaxWidth);
    ASSERT_EQ(s.background.height.min, kMinHeight);
    ASSERT_EQ(s.background.height.max, kMaxHeight / kSampleAspectRatio);

    // Validate old logic consistency.
    const QSize minSize = core::Geometry::expanded(
        kSampleAspectRatio,
        qnGlobals->layoutBackgroundMinSize(),
        Qt::KeepAspectRatioByExpanding).toSize();
    const QSize maxSize = core::Geometry::expanded(
        kSampleAspectRatio,
        qnGlobals->layoutBackgroundMaxSize(),
        Qt::KeepAspectRatio).toSize();
    ASSERT_EQ(s.background.width.min, minSize.width());
    ASSERT_EQ(s.background.width.max, maxSize.width());
    ASSERT_EQ(s.background.height.min, minSize.height());
    ASSERT_EQ(s.background.height.max, maxSize.height());


    s = Reducer::setKeepImageAspectRatio(std::move(s), false);
    ASSERT_EQ(s.background.width.min, kMinWidth);
    ASSERT_EQ(s.background.width.max, kMaxWidth);
    ASSERT_EQ(s.background.height.min, kMinHeight);
    ASSERT_EQ(s.background.height.max, kMaxHeight);
}

TEST_F(LayoutSettingsDialogStateReducerTest, checkFixMinimumBackgroundSize)
{
    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setBackgroundSize({1, 1});

    State s;
    s = Reducer::loadLayout(std::move(s), layout);
    const int kMinWidth = qnGlobals->layoutBackgroundMinSize().width();
    const int kMinHeight = qnGlobals->layoutBackgroundMinSize().height();
    ASSERT_EQ(s.background.width.value, kMinWidth);
    ASSERT_EQ(s.background.height.value, kMinHeight);
}


} // namespace desktop
} // namespace client
} // namespace nx
