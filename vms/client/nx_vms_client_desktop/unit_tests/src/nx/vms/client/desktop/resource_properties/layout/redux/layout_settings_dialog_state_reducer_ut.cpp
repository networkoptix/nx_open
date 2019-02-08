#include <gtest/gtest.h>

#include <nx/client/core/utils/geometry.h>
#include <core/resource/layout_resource.h>

#include <nx/vms/client/desktop/resource_properties/layout/redux/layout_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/layout/redux/layout_settings_dialog_state_reducer.h>

#include <ui/style/globals.h>

namespace nx::vms::client::desktop {

using State = LayoutSettingsDialogState;
using Reducer = LayoutSettingsDialogStateReducer;

class LayoutSettingsDialogStateReducerTest: public testing::Test
{
protected:
    /**
     * Sets up the stuff shared by all tests in this test case.
     * Google Test will call SetUpTestCase() before running the first test in the test case.
     */
    static void SetUpTestCase()
    {
        Reducer::setTracingEnabled(true);
    }

    /**
     * Setup the test fixture. Will be called before each fixture is run.
     */
    virtual void SetUp() override
    {
        Reducer::setScreenAspectRatio({16, 9});
        m_globals.reset(new QnGlobals());
    }

    /**
     * Tears down the test fixture. Will be called after each fixture is run.
     */
    virtual void TearDown() override
    {
        m_globals.reset();
    }

    static QImage makeImage(int width, int height)
    {
        return QImage(width, height, QImage::Format::Format_Mono);
    }

    static QImage makeImage(float aspectRatio)
    {
        constexpr int kSampleHeight = 1000;
        return makeImage(kSampleHeight * aspectRatio, kSampleHeight);
    }

    // Emulates loading a new image into a clean layout.
    static State makeImageState(float imageAspectRatio, float cellAspectRatio = 1.0)
    {
        QnLayoutResourcePtr layout(new QnLayoutResource());
        layout->setCellAspectRatio(cellAspectRatio);
        layout->setCellSpacing(0);

        State state = Reducer::loadLayout(State(), layout);
        state = Reducer::clearBackgroundImage(std::move(state));
        state = Reducer::imageSelected(std::move(state), "/test.jpg");
        state = Reducer::setPreview(std::move(state), makeImage(imageAspectRatio));
        return state;
    }

    static bool rangeLimitsAreValid(const State::Range& range, bool precise = false)
    {
        if (range.value < range.min || range.value > range.max)
            return false;

        if (precise)
        {
            return range.min == State::kBackgroundMinSize
                && range.max == State::kBackgroundMaxSize;

        }

        return range.min >= State::kBackgroundMinSize
            && range.max <= State::kBackgroundMaxSize;
    }

    static bool rangeValueIs(const State::Range& range, int value)
    {
        return rangeLimitsAreValid(range) && range.value == value;
    }

    static QSize backgroundSize(const State& state)
    {
        if (!rangeLimitsAreValid(state.background.width)
            || !rangeLimitsAreValid(state.background.height))
        {
            return QSize();
        }

        return {state.background.width.value, state.background.height.value};
    }

private:
    QScopedPointer<QnGlobals> m_globals;
};

namespace {

// Constants are selected manually and depend on the hardcoded limits.
static constexpr int kSquareAspectRatio = 1;
static constexpr int kSquareSideLength = 40;
static_assert(kSquareSideLength * kSquareSideLength == State::kBackgroundRecommendedArea);
static constexpr QSize kSquareBackgroundSize(kSquareSideLength, kSquareSideLength);

static constexpr int kWideAspectRatio = 2;
static constexpr int kUltraWideAspectRatio = 4;

// Select maximum values which keep the given aspect ratio and fit into recommended area.
static constexpr int kWideShortSideLength = 28;
static constexpr int kWideLongSideLength = 56;
static_assert(kWideLongSideLength / kWideShortSideLength == kWideAspectRatio);
static_assert(kWideShortSideLength * kWideLongSideLength <= State::kBackgroundRecommendedArea);
static_assert(
    (kWideShortSideLength + 1) * (kWideLongSideLength + 2) > State::kBackgroundRecommendedArea);

static constexpr QSize kWideBackgroundSize(kWideLongSideLength, kWideShortSideLength);
static constexpr QSize kUltraWideBackgroundSize(
    State::kBackgroundMaxSize,
    State::kBackgroundMaxSize / kUltraWideAspectRatio);

static constexpr qreal kNarrowAspectRatio = 1.0 / kWideAspectRatio;
static constexpr QSize kNarrowBackgroundSize(kWideShortSideLength, kWideLongSideLength);
static constexpr QSize kUltraNarrowBackgroundSize(
    State::kBackgroundMaxSize / kUltraWideAspectRatio,
    State::kBackgroundMaxSize);

} // namespace

TEST_F(LayoutSettingsDialogStateReducerTest, defaultSize)
{
    State state = Reducer::clearBackgroundImage(State());
    ASSERT_EQ(backgroundSize(state), QSize(State::kBackgroundMinSize, State::kBackgroundMinSize));
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

    State s = makeImageState(kSampleAspectRatio);
    s.background.keepImageAspectRatio = true;
    s.background.cropToMonitorAspectRatio = false;

    s = Reducer::setKeepImageAspectRatio(std::move(s), true);
    ASSERT_EQ(s.background.width.min, State::kBackgroundMinSize * kSampleAspectRatio);
    ASSERT_EQ(s.background.width.max, State::kBackgroundMaxSize);
    ASSERT_EQ(s.background.height.min, State::kBackgroundMinSize);
    ASSERT_EQ(s.background.height.max, State::kBackgroundMaxSize / kSampleAspectRatio);

    // Validate old logic consistency.
    const QSize minSize = core::Geometry::expanded(
        kSampleAspectRatio,
        QSize(State::kBackgroundMinSize, State::kBackgroundMinSize),
        Qt::KeepAspectRatioByExpanding).toSize();
    const QSize maxSize = core::Geometry::expanded(
        kSampleAspectRatio,
        QSize(State::kBackgroundMaxSize, State::kBackgroundMaxSize),
        Qt::KeepAspectRatio).toSize();

    ASSERT_EQ(s.background.width.min, minSize.width());
    ASSERT_EQ(s.background.width.max, maxSize.width());
    ASSERT_EQ(s.background.height.min, minSize.height());
    ASSERT_EQ(s.background.height.max, maxSize.height());

    s = Reducer::setKeepImageAspectRatio(std::move(s), false);
    ASSERT_TRUE(rangeLimitsAreValid(s.background.width, /*precise*/ true));
    ASSERT_TRUE(rangeLimitsAreValid(s.background.height, /*precise*/ true));
}

TEST_F(LayoutSettingsDialogStateReducerTest, checkFixMinimumBackgroundSize)
{
    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setBackgroundSize({1, 1});

    ASSERT_EQ(backgroundSize(Reducer::loadLayout(State(), layout)),
        QSize(State::kBackgroundMinSize, State::kBackgroundMinSize));
}

TEST_F(LayoutSettingsDialogStateReducerTest, checkSetFixedSize)
{
    State s = Reducer::setFixedSizeEnabled(State(), true);
    s = Reducer::setFixedSizeWidth(std::move(s), 2);
    ASSERT_FALSE(s.fixedSize.isEmpty());
}

TEST_F(LayoutSettingsDialogStateReducerTest, setRecommendedAreaOnNewImage)
{
    // Square image.
    ASSERT_EQ(backgroundSize(makeImageState(kSquareAspectRatio, kWideAspectRatio)),
        kNarrowBackgroundSize);

    ASSERT_EQ(backgroundSize(makeImageState(kSquareAspectRatio, kSquareAspectRatio)),
        kSquareBackgroundSize);

    ASSERT_EQ(backgroundSize(makeImageState(kSquareAspectRatio, kNarrowAspectRatio)),
        kWideBackgroundSize);

    // Wide image.
    ASSERT_EQ(backgroundSize(makeImageState(kWideAspectRatio, kWideAspectRatio)),
        kSquareBackgroundSize);

    ASSERT_EQ(backgroundSize(makeImageState(kWideAspectRatio, kSquareAspectRatio)),
        kWideBackgroundSize);

    ASSERT_EQ(backgroundSize(makeImageState(kWideAspectRatio, kNarrowAspectRatio)),
        kUltraWideBackgroundSize);

    // Narrow image.
    ASSERT_EQ(backgroundSize(makeImageState(kNarrowAspectRatio, kWideAspectRatio)),
        kUltraNarrowBackgroundSize);

    ASSERT_EQ(backgroundSize(makeImageState(kNarrowAspectRatio, kSquareAspectRatio)),
        kNarrowBackgroundSize);

    ASSERT_EQ(backgroundSize(makeImageState(kNarrowAspectRatio, kNarrowAspectRatio)),
        kSquareBackgroundSize);
}

TEST_F(LayoutSettingsDialogStateReducerTest, keepAreaOnImageChange)
{
    static const QSize kExampleBackgroundSize(10, 10);

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setCellAspectRatio(1.0);
    layout->setBackgroundSize(kExampleBackgroundSize);
    layout->setBackgroundImageFilename("test.jpg");

    State state = Reducer::loadLayout(State(), layout);
    state = Reducer::startDownloading(std::move(state), "/test.jpg");
    state = Reducer::imageDownloaded(std::move(state));
    state = Reducer::setPreview(std::move(state), makeImage(kSquareAspectRatio));

    ASSERT_EQ(backgroundSize(state), kExampleBackgroundSize);
}

} // namespace nx::vms::client::desktop
