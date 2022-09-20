// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_settings_dialog_state_reducer.h"

#include <set>

#include <QtCore/QtMath>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>

#include <ui/common/image_processing.h>

#include <utils/common/aspect_ratio.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/optional.h>

namespace nx::vms::client::desktop {

using State = LayoutSettingsDialogState;
using BackgroundImageStatus = State::BackgroundImageStatus;

namespace {

// If difference between image AR and screen AR is smaller than this value, cropped preview will
// not be generated.
const qreal kAspectRatioVariation = 0.05;

constexpr int kDefaultBackgroundOpacityPercent = 70;

static bool tracingEnabled(false);

static const nx::utils::log::Tag kLogTag(typeid(LayoutSettingsDialogStateReducer));
static const nx::utils::log::Filter kLogFilter(kLogTag);

// Default screen aspect ratio. Actual for screens of 1920*1080 and so on.
static QnAspectRatio screenAspectRatioValue{16, 9};

static bool keepBackgroundAspectRatioValue(true);

int boundOpacityPercent(int value)
{
    return qBound(1, value, 100);
}

int boundBgRangeValue(int value)
{
    return qBound(State::kBackgroundMinSize, value, State::kBackgroundMaxSize);
}

// Aspect ratio that is optimal for cells to best fit the current image.
std::optional<qreal> bestAspectRatioForCells(const State& state)
{
    const auto& background = state.background;

    switch (background.status)
    {
        case BackgroundImageStatus::loaded:
        case BackgroundImageStatus::newImageLoaded:
            break;
        default:
            return std::nullopt;
    }

    QImage image = (background.canChangeAspectRatio() && background.cropToMonitorAspectRatio)
        ? background.croppedPreview
        : background.preview;

    if (image.isNull())
        return std::nullopt;

    const qreal aspectRatio = (qreal)image.width() / (qreal)image.height();
    const qreal cellAspectRatio = qFuzzyIsNull(state.cellAspectRatio)
        ? QnLayoutResource::kDefaultCellAspectRatio
        : state.cellAspectRatio;

    return aspectRatio / cellAspectRatio;
}

// Returns true if width and height in cells are already set to values corresponding to
// bestAspectRatioForCells()
bool cellsAreBestAspected(const State& state)
{
    if (!state.background.keepImageAspectRatio)
        return true;

    const auto targetAspectRatio = bestAspectRatioForCells(state);
    if (!targetAspectRatio)
        return true;

    int w = qRound((qreal)state.background.height.value * (*targetAspectRatio));
    int h = qRound((qreal)state.background.width.value / (*targetAspectRatio));
    return (boundBgRangeValue(w) == state.background.width.value
        || boundBgRangeValue(h) == state.background.height.value);
}

void updateBackgroundLimits(State& state)
{
    const auto targetAspectRatio = bestAspectRatioForCells(state);
    if (state.background.keepImageAspectRatio && targetAspectRatio)
    {
        const qreal value = *targetAspectRatio;
        if (value < 1.0)
        {
            state.background.width.setRange(
                State::kBackgroundMinSize,
                boundBgRangeValue((int) (State::kBackgroundMaxSize * value)));
            state.background.height.setRange(
                boundBgRangeValue((int) (State::kBackgroundMinSize / value)),
                State::kBackgroundMaxSize);
        }
        else
        {
            state.background.width.setRange(
                boundBgRangeValue((int) (State::kBackgroundMinSize * value)),
                State::kBackgroundMaxSize);
            state.background.height.setRange(
                State::kBackgroundMinSize,
                boundBgRangeValue((int) (State::kBackgroundMaxSize / value)));
        }
    }
    else
    {
        state.background.width.setRange(State::kBackgroundMinSize, State::kBackgroundMaxSize);
        state.background.height.setRange(State::kBackgroundMinSize, State::kBackgroundMaxSize);
    }
}

void resetBackgroundParameters(State& state)
{
    state.background = {};
    updateBackgroundLimits(state);
    state.background.width.value = state.background.width.min;
    state.background.height.value = state.background.height.min;
    state.background.opacityPercent = kDefaultBackgroundOpacityPercent;
}

void applyRecommendedBackgroundSize(State& state)
{
    const auto targetAspectRatio = bestAspectRatioForCells(state);
    NX_ASSERT(targetAspectRatio);
    if (!targetAspectRatio)
        return;

    const auto aspectRatio = *targetAspectRatio;

    // Limit w*h <= recommended area; minor variations are allowed, e.g. 17*6 ~~= 100.
    qreal height = qSqrt(State::kBackgroundRecommendedArea / aspectRatio);
    qreal width = height * aspectRatio;

    if (height > State::kBackgroundMaxSize)
    {
        height = State::kBackgroundMaxSize;
        width = height * aspectRatio;
    }
    else if (width > State::kBackgroundMaxSize)
    {
        width = State::kBackgroundMaxSize;
        height = width / aspectRatio;
    }

    state.background.width.setValue(width);
    state.background.height.setValue(height);
}

int findFreeLogicalId(const std::set<int>& usedValues)
{
    int previousValue = 0;
    for (auto value: usedValues)
    {
        if (value > previousValue + 1)
            break;
        previousValue = value;
    }
    return previousValue + 1;
}

void trace(const State& state, QString message)
{
    if (tracingEnabled)
    {
        NX_DEBUG(kLogTag, message);
        NX_DEBUG(kLogTag, state);
    }
}

} // namespace

bool LayoutSettingsDialogStateReducer::isTracingEnabled()
{
    return tracingEnabled;
}

void LayoutSettingsDialogStateReducer::setTracingEnabled(bool value)
{
    if (tracingEnabled == value)
        return;

    tracingEnabled = value;

    using namespace nx::utils::log;
    if (value)
    {
        addLogger(std::make_unique<Logger>(
            std::set<Filter>{kLogFilter}, Level::verbose, std::make_unique<StdOut>()));
    }
    else
    {
         removeLoggers({kLogFilter});
    }
}

QnAspectRatio LayoutSettingsDialogStateReducer::screenAspectRatio()
{
    return screenAspectRatioValue;
}

void LayoutSettingsDialogStateReducer::setScreenAspectRatio(QnAspectRatio value)
{
    screenAspectRatioValue = value;
}

bool LayoutSettingsDialogStateReducer::keepBackgroundAspectRatio()
{
    return keepBackgroundAspectRatioValue;
}

void LayoutSettingsDialogStateReducer::setKeepBackgroundAspectRatio(bool value)
{
    keepBackgroundAspectRatioValue = value;
}

State LayoutSettingsDialogStateReducer::loadLayout(State state, const QnLayoutResourcePtr& layout)
{
    trace(state, "loadLayout");
    state.locked = layout->locked();
    state.logicalId = layout->logicalId();
    state.isLocalFile = layout->isFile();
    state.isCrossSystem = layout->hasFlags(Qn::cross_system);

    state.fixedSize = layout->fixedSize();
    state.fixedSizeEnabled = !state.fixedSize.isEmpty();


    if (layout->hasCellAspectRatio())
    {
        const auto spacing = layout->cellSpacing();
        const qreal cellWidth = 1.0 + spacing;
        const qreal cellHeight = 1.0 / layout->cellAspectRatio() + spacing;
        state.cellAspectRatio = cellWidth / cellHeight;
    }
    else
    {
        state.cellAspectRatio = QnLayoutResource::kDefaultCellAspectRatio;
    }

    resetBackgroundParameters(state);
    state.background.supported = !state.isCrossSystem;
    if (state.background.supported)
    {
        state.background.filename = layout->backgroundImageFilename();
        state.background.width.setValue(layout->backgroundSize().width());
        state.background.height.setValue(layout->backgroundSize().height());
        state.background.opacityPercent = boundOpacityPercent(
            int(layout->backgroundOpacity() * 100));
        updateBackgroundLimits(state);
    }

    return state;
}

State LayoutSettingsDialogStateReducer::setLocked(State state, bool value)
{
    trace(state, "setLocked");
    state.locked = value;
    return state;
}

State LayoutSettingsDialogStateReducer::setLogicalId(State state, int value)
{
    trace(state, "setLogicalId");
    state.logicalId = value;
    return state;
}

State LayoutSettingsDialogStateReducer::setOtherLogicalIds(State state, const std::set<int>& value)
{
    trace(state, "setOtherLogicalIds");
    state.otherLogicalIds = value;
    return state;
}

State LayoutSettingsDialogStateReducer::resetLogicalId(State state)
{
    trace(state, "resetLogicalId");
    state.logicalId = 0;
    return state;
}

State LayoutSettingsDialogStateReducer::generateLogicalId(State state)
{
    trace(state, "generateLogicalId");
    state.logicalId = findFreeLogicalId(state.otherLogicalIds);
    return state;
}

State LayoutSettingsDialogStateReducer::setFixedSizeEnabled(State state, bool value)
{
    trace(state, "setFixedSizeEnabled");
    state.fixedSizeEnabled = value;
    if (value && state.fixedSize.isEmpty())
        state.fixedSize = {1, 1};
    return state;
}

State LayoutSettingsDialogStateReducer::setFixedSizeWidth(State state, int value)
{
    trace(state, "setFixedSizeWidth");
    state.fixedSize.setWidth(value);
    return state;
}

State LayoutSettingsDialogStateReducer::setFixedSizeHeight(State state, int value)
{
    trace(state, "setFixedSizeHeight");
    state.fixedSize.setHeight(value);
    return state;
}

State LayoutSettingsDialogStateReducer::setBackgroundImageError(State state,
    const QString& errorText)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "setBackgroundImageError");
    state.background.status = BackgroundImageStatus::error;
    state.background.errorText = errorText;
    return state;
}

State LayoutSettingsDialogStateReducer::clearBackgroundImage(State state)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "clearBackgroundImage");
    resetBackgroundParameters(state);
    return state;
}

State LayoutSettingsDialogStateReducer::setBackgroundImageOpacityPercent(State state, int value)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "setBackgroundImageOpacityPercent");
    state.background.opacityPercent = boundOpacityPercent(value);
    return state;
}

State LayoutSettingsDialogStateReducer::setBackgroundImageWidth(State state, int value)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "setBackgroundImageWidth");
    state.background.width.setValue(value);
    if (state.background.keepImageAspectRatio)
    {
        const auto targetAspectRatio = bestAspectRatioForCells(state);
        if (targetAspectRatio)
        {
            int h = qRound((qreal)value / *targetAspectRatio);
            state.background.height.setValue(h);
        }
    }
    return state;
}

State LayoutSettingsDialogStateReducer::setBackgroundImageHeight(State state, int value)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "setBackgroundImageHeight");
    state.background.height.setValue(value);
    if (state.background.keepImageAspectRatio)
    {
        const auto targetAspectRatio = bestAspectRatioForCells(state);
        if (targetAspectRatio)
        {
            int w = qRound((qreal)value * (*targetAspectRatio));
            state.background.width.setValue(w);
        }
    }
    return state;
}

State LayoutSettingsDialogStateReducer::setCropToMonitorAspectRatio(State state, bool value)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "setCropToMonitorAspectRatio");
    state.background.cropToMonitorAspectRatio = value;
    updateBackgroundLimits(state);
    if (!cellsAreBestAspected(state))
        applyRecommendedBackgroundSize(state);
    return state;
}

State LayoutSettingsDialogStateReducer::setKeepImageAspectRatio(State state, bool value)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "setKeepImageAspectRatio");
    state.background.keepImageAspectRatio = value;
    updateBackgroundLimits(state);
    if (!cellsAreBestAspected(state))
        applyRecommendedBackgroundSize(state);
    return state;
}

State LayoutSettingsDialogStateReducer::startDownloading(State state, const QString& targetPath)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "startDownloading");
    NX_ASSERT(state.background.canStartDownloading());
    state.background.imageSourcePath = targetPath;
    state.background.status = BackgroundImageStatus::downloading;
    return state;
}

State LayoutSettingsDialogStateReducer::imageDownloaded(State state)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "imageDownloaded");
    NX_ASSERT(state.background.status == BackgroundImageStatus::downloading);
    state.background.status = BackgroundImageStatus::loading;
    return state;
}

State LayoutSettingsDialogStateReducer::imageSelected(State state, const QString& filename)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "imageSelected");
    state.background.status = BackgroundImageStatus::newImageLoading;
    state.background.imageSourcePath = filename;
    state.background.filename = QString();
    state.background.preview = QImage();
    state.background.croppedPreview = QImage();
    state.background.errorText = QString();
    return state;
}

State LayoutSettingsDialogStateReducer::startUploading(State state)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "startUploading");
    // We can re-upload existing image if it was cropped.
    NX_ASSERT(state.background.status == BackgroundImageStatus::loaded
        || state.background.status == BackgroundImageStatus::newImageLoaded);
    state.background.status = BackgroundImageStatus::newImageUploading;
    return state;
}

State LayoutSettingsDialogStateReducer::imageUploaded(State state, const QString& filename)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "imageUploaded");
    state.background.filename = filename;
    state.background.status = BackgroundImageStatus::newImageLoaded;
    return state;
}

State LayoutSettingsDialogStateReducer::setPreview(State state, const QImage& image)
{
    if (!NX_ASSERT(state.background.supported))
        return state;

    trace(state, "setPreview");
    if (state.background.status == BackgroundImageStatus::loading)
        state.background.status = BackgroundImageStatus::loaded;
    else if (state.background.status == BackgroundImageStatus::newImageLoading)
        state.background.status = BackgroundImageStatus::newImageLoaded;

    state.background.preview = image;

    // Disable cropping for images that are quite well aspected.
    QnAspectRatio imageAspectRatio(image.width(), image.height());
    if (qAbs(imageAspectRatio.toFloat() - screenAspectRatio().toFloat()) > kAspectRatioVariation)
    {
        state.background.croppedPreview = cropToAspectRatio(image, screenAspectRatio());
    }
    else
    {
        state.background.croppedPreview = QImage();
    }

    if (state.background.status == BackgroundImageStatus::newImageLoaded)
    {
        // Always set flag to true for new images.
        state.background.keepImageAspectRatio = true;

        // Adjust background size to the recommended values.
        applyRecommendedBackgroundSize(state);

    }
    else if (!keepBackgroundAspectRatio())
    {
        // Keep false if previous accepted value was false.
        state.background.keepImageAspectRatio = false;
    }
    else
    {
        // Set to true if possible (current sizes will not be changed).
        state.background.keepImageAspectRatio = cellsAreBestAspected(state);
    }
    updateBackgroundLimits(state);

    return state;
}

} // namespace nx::vms::client::desktop
