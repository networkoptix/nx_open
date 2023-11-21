// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/export/dialogs/private/export_settings_dialog_state.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

namespace nx::vms::client::desktop {
namespace test {

namespace {

} // namespace

class ExportSettingsDialogStateReducerTest: public ContextBasedTest
{
public:
    using State = ExportSettingsDialogState;
    using Reducer = ExportSettingsDialogStateReducer;

protected:
    ExportSettingsDialogStateReducerTest() : state(QSize{320, 240}, QString(), QString()) {}

    State state;
};

// Enable both tabs.
TEST_F(ExportSettingsDialogStateReducerTest, enableTabs)
{
    ASSERT_FALSE(state.mediaAvailable);
    ASSERT_FALSE(state.layoutAvailable);

    state = Reducer::enableTab(std::move(state), ExportMode::media);

    ASSERT_TRUE(state.mediaAvailable);
    ASSERT_FALSE(state.layoutAvailable);

    state = Reducer::enableTab(std::move(state), ExportMode::layout);

    ASSERT_TRUE(state.mediaAvailable);
    ASSERT_TRUE(state.layoutAvailable);
}

// Forbid switch to media mode if its tab is not available or disabled.
TEST_F(ExportSettingsDialogStateReducerTest, forbidMediaMode)
{
    ASSERT_FALSE(state.mediaAvailable);
    ASSERT_FALSE(state.layoutAvailable);
    ASSERT_EQ(ExportMode::media, state.mode);

    state = Reducer::enableTab(std::move(state), ExportMode::layout);
    ASSERT_TRUE(state.layoutAvailable);

    state = Reducer::setMode(std::move(state), ExportMode::layout).second;
    ASSERT_EQ(ExportMode::layout, state.mode);

    state = Reducer::setMode(std::move(state), ExportMode::media).second;
    ASSERT_EQ(ExportMode::layout, state.mode);

    state = Reducer::disableTab(std::move(state), ExportMode::media, "reason");
    state = Reducer::setMode(std::move(state), ExportMode::media).second;
    ASSERT_EQ(ExportMode::layout, state.mode);
}

// Forbid switch to layout mode if its tab is not available or disabled.
TEST_F(ExportSettingsDialogStateReducerTest, forbidLayoutMode)
{
    ASSERT_FALSE(state.mediaAvailable);
    ASSERT_FALSE(state.layoutAvailable);
    ASSERT_EQ(ExportMode::media, state.mode);

    state = Reducer::enableTab(std::move(state), ExportMode::media);
    ASSERT_TRUE(state.mediaAvailable);

    state = Reducer::setMode(std::move(state), ExportMode::layout).second;
    ASSERT_EQ(ExportMode::media, state.mode);

    state = Reducer::disableTab(std::move(state), ExportMode::layout, "reason");
    state = Reducer::setMode(std::move(state), ExportMode::layout).second;
    ASSERT_EQ(ExportMode::media, state.mode);
}

// Enable tab after disabling with reason.
TEST_F(ExportSettingsDialogStateReducerTest, disableEnableTab)
{
    ASSERT_FALSE(state.mediaAvailable);
    ASSERT_FALSE(state.layoutAvailable);

    state = Reducer::disableTab(std::move(state), ExportMode::media, "reason");

    ASSERT_TRUE(state.mediaAvailable);
    ASSERT_EQ("reason", state.mediaDisableReason);
    ASSERT_FALSE(state.layoutAvailable);
    ASSERT_TRUE(state.layoutDisableReason.isNull());

    state = Reducer::enableTab(std::move(state), ExportMode::media);

    ASSERT_TRUE(state.mediaAvailable);
    ASSERT_TRUE(state.mediaDisableReason.isNull());
    ASSERT_FALSE(state.layoutAvailable);
    ASSERT_TRUE(state.layoutDisableReason.isNull());
}

// Overlays are applied in the order they were selected.
TEST_F(ExportSettingsDialogStateReducerTest, selectOverlayOrder)
{
    ASSERT_EQ(state.exportMediaPersistentSettings.usedOverlays, QVector<ExportOverlayType>());

    state = Reducer::selectOverlay(std::move(state), ExportOverlayType::text);
    ASSERT_EQ(state.exportMediaPersistentSettings.usedOverlays,
        QVector<ExportOverlayType>({
            ExportOverlayType::text}));

    state = Reducer::selectOverlay(std::move(state), ExportOverlayType::text);
    ASSERT_EQ(state.exportMediaPersistentSettings.usedOverlays,
        QVector<ExportOverlayType>({
            ExportOverlayType::text}));

    state = Reducer::selectOverlay(std::move(state), ExportOverlayType::image);
    ASSERT_EQ(state.exportMediaPersistentSettings.usedOverlays,
        QVector<ExportOverlayType>({
            ExportOverlayType::text,
            ExportOverlayType::image}));

    state = Reducer::selectOverlay(std::move(state), ExportOverlayType::text);
    ASSERT_EQ(state.exportMediaPersistentSettings.usedOverlays,
        QVector<ExportOverlayType>({
            ExportOverlayType::image,
            ExportOverlayType::text}));

    state = Reducer::selectOverlay(std::move(state), ExportOverlayType::info);
    ASSERT_EQ(state.exportMediaPersistentSettings.usedOverlays,
        QVector<ExportOverlayType>({
            ExportOverlayType::image,
            ExportOverlayType::text,
            ExportOverlayType::info}));

    state = Reducer::selectOverlay(std::move(state), ExportOverlayType::bookmark);
    ASSERT_EQ(state.exportMediaPersistentSettings.usedOverlays,
        QVector<ExportOverlayType>({
            ExportOverlayType::image,
            ExportOverlayType::text,
            ExportOverlayType::info,
            ExportOverlayType::bookmark}));

    state = Reducer::hideOverlay(std::move(state), ExportOverlayType::image);
    ASSERT_EQ(state.exportMediaPersistentSettings.usedOverlays,
        QVector<ExportOverlayType>({
            ExportOverlayType::text,
            ExportOverlayType::info,
            ExportOverlayType::bookmark}));
}

// Switching selection between overlays and rapid review.
TEST_F(ExportSettingsDialogStateReducerTest, overlayAndRapidReviewSelection)
{
    ASSERT_EQ(ExportOverlayType::none, state.selectedOverlayType);
    ASSERT_FALSE(state.rapidReviewSelected);

    state = Reducer::selectOverlay(std::move(state), ExportOverlayType::text);
    ASSERT_EQ(ExportOverlayType::text, state.selectedOverlayType);
    ASSERT_FALSE(state.rapidReviewSelected);

    state = Reducer::selectRapidReview(std::move(state));
    ASSERT_EQ(ExportOverlayType::none, state.selectedOverlayType);
    ASSERT_TRUE(state.rapidReviewSelected);

    state = Reducer::selectOverlay(std::move(state), ExportOverlayType::none);
    ASSERT_EQ(ExportOverlayType::none, state.selectedOverlayType);
    ASSERT_TRUE(state.rapidReviewSelected);

    state = Reducer::selectOverlay(std::move(state), ExportOverlayType::image);
    ASSERT_EQ(ExportOverlayType::image, state.selectedOverlayType);
    ASSERT_FALSE(state.rapidReviewSelected);

    state = Reducer::clearSelection(std::move(state));
    ASSERT_EQ(ExportOverlayType::none, state.selectedOverlayType);
    ASSERT_FALSE(state.rapidReviewSelected);
}

// Overlays are not selected for nov files.
TEST_F(ExportSettingsDialogStateReducerTest, overlayNotSelectedForNov)
{
    ASSERT_FALSE(state.mediaAvailable);
    ASSERT_FALSE(state.layoutAvailable);
    state = Reducer::setMediaFilename(std::move(state), Filename::parse("test.mp4"));

    state = Reducer::selectOverlay(std::move(state), ExportOverlayType::text);
    ASSERT_EQ(ExportOverlayType::text, state.selectedOverlayType);

    state = Reducer::setMediaFilename(std::move(state), Filename::parse("test.nov"));
    ASSERT_EQ(ExportOverlayType::none, state.selectedOverlayType);
}

// Rapid review is not selected for nov files.
TEST_F(ExportSettingsDialogStateReducerTest, rapidReviewNotSelectedForNov)
{
    ASSERT_FALSE(state.mediaAvailable);
    ASSERT_FALSE(state.layoutAvailable);
    state = Reducer::setMediaFilename(std::move(state), Filename::parse("test.avi"));

    state = Reducer::selectRapidReview(std::move(state));
    ASSERT_TRUE(state.rapidReviewSelected);

    state = Reducer::setMediaFilename(std::move(state), Filename::parse("test.nov"));
    ASSERT_EQ(ExportOverlayType::none, state.selectedOverlayType);
    ASSERT_FALSE(state.rapidReviewSelected);
}

} // namespace test
} // namespace nx::vms::client::desktop
