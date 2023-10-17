// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource_management/resource_pool.h>
#include <nx/core/access/access_types.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource_properties/layout/flux/layout_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/layout/flux/layout_settings_dialog_state_reducer.h>
#include <nx/vms/client/desktop/resource_properties/layout/flux/layout_settings_dialog_store.h>
#include <nx/vms/client/desktop/resource_properties/layout/watchers/layout_logical_ids_watcher.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

namespace nx::vms::client::desktop {
namespace test {

using State = LayoutSettingsDialogState;
using Store = LayoutSettingsDialogStore;
using Reducer = LayoutSettingsDialogStateReducer;

struct BackgroundSetup
{
    const qreal imageAspectRatio = 0;
    const qreal cellAspectRatio = 0;
    const State::Range width;
    const State::Range height;
    const QSize backgroundSize;

    constexpr BackgroundSetup(
        qreal imageAspectRatio,
        qreal cellAspectRatio,
        State::Range width,
        State::Range height)
        :
        imageAspectRatio(imageAspectRatio),
        cellAspectRatio(cellAspectRatio),
        width(width),
        height(height),
        backgroundSize(width.value, height.value)
    {
    }
};

void PrintTo(BackgroundSetup value, ::std::ostream* os)
{
    *os
        << "( " << value.imageAspectRatio << ", " << value.cellAspectRatio << ", "
        << "w:{" << value.width.min << "-" << value.width.value << "-" << value.width.max <<"}, "
        << "h:{" << value.height.min << "-" << value.height.value << "-" << value.height.max <<"})";
}

namespace {

// Constants are selected manually and depend on the hardcoded limits.
static constexpr qreal kSquareAspectRatio = 1;
static constexpr int kSquareSideLength = 40;
static constexpr State::Range kSquareSide(
    State::kBackgroundMinSize,
    kSquareSideLength,
    State::kBackgroundMaxSize);

static_assert(kSquareSideLength * kSquareSideLength == State::kBackgroundRecommendedArea);

static constexpr qreal kWideAspectRatio = 2;
static constexpr qreal kUltraWideAspectRatio = 4;
static constexpr qreal kNarrowAspectRatio = 1.0 / kWideAspectRatio;
static constexpr qreal kSuperWideAspectRatio = 1000;
static constexpr qreal kSuperNarrowAspectRatio = 1.0 / kSuperWideAspectRatio;

// Select maximum values which keep the given aspect ratio and fit into recommended area.
static constexpr int kWideShortSideLength = 28;
static constexpr State::Range kWideShortSide(
    State::kBackgroundMinSize,
    kWideShortSideLength,
    (int) (State::kBackgroundMaxSize / kWideAspectRatio));

static constexpr int kWideLongSideLength = 56;
static constexpr State::Range kWideLongSide(
    (int) (State::kBackgroundMinSize * kWideAspectRatio),
    kWideLongSideLength,
    State::kBackgroundMaxSize);

static_assert(kWideLongSideLength / kWideShortSideLength == (int) kWideAspectRatio);
static_assert(kWideShortSideLength * kWideLongSideLength <= State::kBackgroundRecommendedArea);
static_assert(
    (kWideShortSideLength + 1) * (kWideLongSideLength + 2) > State::kBackgroundRecommendedArea);

static constexpr State::Range kUltraWideLongSide(
    (int) (State::kBackgroundMinSize * kUltraWideAspectRatio),
    State::kBackgroundMaxSize,
    State::kBackgroundMaxSize);
static constexpr State::Range kUltraWideShortSide(
    State::kBackgroundMinSize,
    (int) (State::kBackgroundMaxSize / kUltraWideAspectRatio),
    (int) (State::kBackgroundMaxSize / kUltraWideAspectRatio)
);

static constexpr State::Range kMaximumRange(
    State::kBackgroundMaxSize,
    State::kBackgroundMaxSize,
    State::kBackgroundMaxSize
);
static constexpr State::Range kMinimumRange(
    State::kBackgroundMinSize,
    State::kBackgroundMinSize,
    State::kBackgroundMinSize);

static const std::vector<BackgroundSetup> kBackgroundSamples = {
    // Square image.
    /*[0]*/ {kSquareAspectRatio, kWideAspectRatio, kWideShortSide, kWideLongSide},
    /*[1]*/ {kSquareAspectRatio, kSquareAspectRatio, kSquareSide, kSquareSide},
    /*[2]*/ {kSquareAspectRatio, kNarrowAspectRatio, kWideLongSide, kWideShortSide},

    // Wide image.
    /*[3]*/ {kWideAspectRatio, kWideAspectRatio, kSquareSide, kSquareSide},
    /*[4]*/ {kWideAspectRatio, kSquareAspectRatio, kWideLongSide, kWideShortSide},
    /*[5]*/ {kWideAspectRatio, kNarrowAspectRatio, kUltraWideLongSide, kUltraWideShortSide},

    // Narrow image.
    /*[6]*/ {kNarrowAspectRatio, kWideAspectRatio, kUltraWideShortSide, kUltraWideLongSide},
    /*[7]*/ {kNarrowAspectRatio, kSquareAspectRatio, kWideShortSide, kWideLongSide},
    /*[8]*/ {kNarrowAspectRatio, kNarrowAspectRatio, kSquareSide, kSquareSide},

    // Super-wide and narrow images.
    /*[9]*/ {kSuperWideAspectRatio, kSquareAspectRatio, kMaximumRange, kMinimumRange},
    /*[10]*/ {kSuperNarrowAspectRatio, kSquareAspectRatio, kMinimumRange, kMaximumRange}
};

QImage makeImage(int width, int height)
{
    return QImage(width, height, QImage::Format::Format_Mono);
}

QImage makeImage(qreal aspectRatio)
{
    constexpr int kSampleHeight = 1000;
    return makeImage((int) (kSampleHeight * aspectRatio), kSampleHeight);
}

// Emulates loading a new image into a clean layout.
State makeImageState(qreal imageAspectRatio, qreal cellAspectRatio)
{
    QnLayoutResourcePtr layout(new LayoutResource());
    layout->setCellAspectRatio(cellAspectRatio);
    layout->setCellSpacing(0);

    State state = Reducer::loadLayout(State(), layout);
    state = Reducer::clearBackgroundImage(std::move(state));
    state = Reducer::imageSelected(std::move(state), "/test.jpg");
    state = Reducer::setPreview(std::move(state), makeImage(imageAspectRatio));
    return state;
}

bool rangeLimitsAreValid(const State::Range& range, bool precise = false)
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

QSize backgroundSize(const State& state)
{
    if (!rangeLimitsAreValid(state.background.width)
        || !rangeLimitsAreValid(state.background.height))
    {
        return QSize();
    }

    return {state.background.width.value, state.background.height.value};
}

} // namespace

class LayoutSettingsDialogStateReducerTest: public ::testing::Test
{
protected:
    /**
     * Setup the test fixture. Will be called before each fixture is run.
     */
    virtual void SetUp() override
    {
        Reducer::setTracingEnabled(true);
        Reducer::setScreenAspectRatio({16, 9});
    }
};

class LayoutSettingsDialogStateReducerBackgroundTest:
    public LayoutSettingsDialogStateReducerTest,
    public ::testing::WithParamInterface<BackgroundSetup>
{
};

class LayoutSettingsDialogStateInteractionTest: public ContextBasedTest
{
protected:
    Store& store() { return *m_store; }
    const State& state() const { return m_store->state(); }

    void loadLayout(const QnLayoutResourcePtr& layout)
    {
        m_store->loadLayout(layout);
        m_logicalIdsWatcher.reset(new LayoutLogicalIdsWatcher(m_store.get(), layout));
    }

    QnLayoutResourcePtr addLayout(const QString& name, int logicalId = 0)
    {
        auto layout = QnLayoutResourcePtr(new LayoutResource());
        layout->setIdUnsafe(QnUuid::createUuid());
        layout->setName(name);
        layout->setLogicalId(logicalId);
        systemContext()->resourcePool()->addResource(layout);
        return layout;
    }

    /**
     * Setup the test fixture. Will be called before each fixture is run.
     */
    virtual void SetUp() override
    {
        Reducer::setTracingEnabled(true);
        Reducer::setScreenAspectRatio({16, 9});
        m_store.reset(new Store());
    }

    /**
     * Tears down the test fixture. Will be called after each fixture is run.
     */
    virtual void TearDown() override
    {
        m_logicalIdsWatcher.reset();
        m_store.reset();
    }

private:
    std::unique_ptr<Store> m_store;
    std::unique_ptr<LayoutLogicalIdsWatcher> m_logicalIdsWatcher;
};

TEST_F(LayoutSettingsDialogStateReducerTest, defaultSize)
{
    State state = Reducer::clearBackgroundImage({});
    ASSERT_EQ(backgroundSize(state), QSize(State::kBackgroundMinSize, State::kBackgroundMinSize));
}

// Checking setHeight and setWidth are keeping target aspectRatio of kSampleAspectRatio.
TEST_F(LayoutSettingsDialogStateReducerTest, keepAspectRatioOnSizeChange)
{
    constexpr int kSampleAspectRatio = 4;

    State s = makeImageState(kSampleAspectRatio, kSquareAspectRatio);
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

    State s = makeImageState(kSampleAspectRatio, kSquareAspectRatio);
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
    QnLayoutResourcePtr layout(new LayoutResource());
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

TEST_P(LayoutSettingsDialogStateReducerBackgroundTest, setRecommendedAreaOnNewImage)
{
    BackgroundSetup setup = GetParam();
    ASSERT_EQ(backgroundSize(makeImageState(setup.imageAspectRatio, setup.cellAspectRatio)),
        setup.backgroundSize);
}

TEST_P(LayoutSettingsDialogStateReducerBackgroundTest, checkInitialLimits)
{
    const BackgroundSetup setup = GetParam();
    const auto state = makeImageState(setup.imageAspectRatio, setup.cellAspectRatio);
    ASSERT_TRUE(state.background.keepImageAspectRatio);
    ASSERT_EQ(state.background.width, setup.width);
    ASSERT_EQ(state.background.height, setup.height);
}

TEST_P(LayoutSettingsDialogStateReducerBackgroundTest, checkLimitsOnKeepArChange)
{
    const BackgroundSetup setup = GetParam();
    State initial = makeImageState(setup.imageAspectRatio, setup.cellAspectRatio);

    State ignoreAspectRatio = Reducer::setKeepImageAspectRatio(std::move(initial), false);
    ASSERT_EQ(ignoreAspectRatio.background.width, State::Range(setup.width.value));
    ASSERT_EQ(ignoreAspectRatio.background.height, State::Range(setup.height.value));

    State keepAspectRatio = Reducer::setKeepImageAspectRatio(std::move(ignoreAspectRatio), true);
    ASSERT_EQ(keepAspectRatio.background.width, setup.width);
    ASSERT_EQ(keepAspectRatio.background.height, setup.height);
}

INSTANTIATE_TEST_SUITE_P(validateBackgroundSetup,
    LayoutSettingsDialogStateReducerBackgroundTest,
    ::testing::ValuesIn(kBackgroundSamples));

TEST_F(LayoutSettingsDialogStateReducerTest, keepAreaOnImageChange)
{
    static const QSize kExampleBackgroundSize(10, 10);

    QnLayoutResourcePtr layout(new LayoutResource());
    layout->setCellAspectRatio(1.0);
    layout->setBackgroundSize(kExampleBackgroundSize);
    layout->setBackgroundImageFilename("test.jpg");

    State state = Reducer::loadLayout(State(), layout);
    state = Reducer::startDownloading(std::move(state), "/test.jpg");
    state = Reducer::imageDownloaded(std::move(state));
    state = Reducer::setPreview(std::move(state), makeImage(kSquareAspectRatio));

    ASSERT_EQ(backgroundSize(state), kExampleBackgroundSize);
}

TEST_F(LayoutSettingsDialogStateInteractionTest, generateLogicalId)
{
    const auto layout1 = addLayout("Layout 1", 1);
    const auto layout3 = addLayout("Layout 3", 3);

    const auto layout2 = addLayout("Layout 2", 0);
    loadLayout(layout2);
    ASSERT_EQ(state().logicalId, 0);
    store().generateLogicalId();
    ASSERT_EQ(state().logicalId, 2);
    ASSERT_FALSE(state().isDuplicateLogicalId());
    layout2->setLogicalId(state().logicalId);
    ASSERT_FALSE(state().isDuplicateLogicalId());

    const auto layout4 = addLayout("Layout 4", 0);
    loadLayout(layout4);
    ASSERT_EQ(state().logicalId, 0);
    store().generateLogicalId();
    ASSERT_EQ(state().logicalId, 4);
    ASSERT_FALSE(state().isDuplicateLogicalId());
    layout4->setLogicalId(state().logicalId);
    ASSERT_FALSE(state().isDuplicateLogicalId());
}

TEST_F(LayoutSettingsDialogStateInteractionTest, logicalIdsWatcherState)
{
    using ids = decltype(state().otherLogicalIds);

    const auto layout1 = addLayout("Layout 1", 1);
    const auto layout2 = addLayout("Layout 2", 0);
    const auto layout3 = addLayout("Layout 3", 3);
    const auto layout4 = addLayout("Layout 4", 2);

    loadLayout(layout1);
    ASSERT_EQ(state().otherLogicalIds, ids({3, 2}));

    loadLayout(layout2);
    ASSERT_EQ(state().otherLogicalIds, ids({1, 2, 3}));

    loadLayout(layout3);
    ASSERT_EQ(state().otherLogicalIds, ids({1, 2}));

    loadLayout(layout4);
    ASSERT_EQ(state().otherLogicalIds, ids({1, 3}));

    loadLayout(layout2);
    ASSERT_EQ(state().otherLogicalIds, ids({1, 2, 3}));
    ASSERT_FALSE(state().isDuplicateLogicalId());
    store().setLogicalId(2);
    ASSERT_EQ(state().logicalId, 2);
    ASSERT_TRUE(state().isDuplicateLogicalId());

    layout4->resourcePool()->removeResource(layout4);
    ASSERT_EQ(state().otherLogicalIds, ids({1, 3}));
    ASSERT_FALSE(state().isDuplicateLogicalId());
    layout2->setLogicalId(state().logicalId);
    ASSERT_EQ(state().otherLogicalIds, ids({1, 3}));
    ASSERT_FALSE(state().isDuplicateLogicalId());

    auto layout5 = addLayout("Layout 5", 5);
    ASSERT_EQ(state().otherLogicalIds, ids({1, 3, 5}));

    loadLayout(layout5);
    ASSERT_EQ(state().otherLogicalIds, ids({1, 2, 3}));
}

} // namespace test
} // namespace nx::vms::client::desktop
