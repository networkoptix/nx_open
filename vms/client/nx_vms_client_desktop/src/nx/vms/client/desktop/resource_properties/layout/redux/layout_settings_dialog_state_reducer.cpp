#include "layout_settings_dialog_state_reducer.h"

#include <QtCore/QtMath>

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>

#include <ui/common/image_processing.h>
#include <ui/style/globals.h>

#include <utils/common/aspect_ratio.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/std/optional.h>
#include <client/client_settings.h>
#include <set>

namespace nx::vms::client::desktop {

using State = LayoutSettingsDialogState;
using BackgroundImageStatus = State::BackgroundImageStatus;

namespace {

// If difference between image AR and screen AR is smaller than this value, cropped preview will
// not be generated.
const qreal kAspectRatioVariation = 0.05;

constexpr int kDefaultBackgroundOpacityPercent = 70;

int boundOpacityPercent(int value)
{
    return qBound(1, value, 100);
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
        ? qnGlobals->defaultLayoutCellAspectRatio()
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
    return (w == state.background.width.value || h == state.background.height.value);
}

State updateBackgroundLimits(State state)
{
    const int kMinWidth = qnGlobals->layoutBackgroundMinSize().width();
    const int kMaxWidth = qnGlobals->layoutBackgroundMaxSize().width();
    const int kMinHeight = qnGlobals->layoutBackgroundMinSize().height();
    const int kMaxHeight = qnGlobals->layoutBackgroundMaxSize().height();

    const auto targetAspectRatio = bestAspectRatioForCells(state);
    if (state.background.keepImageAspectRatio && targetAspectRatio)
    {
        state.background.width.setRange(kMinWidth * (*targetAspectRatio), kMaxWidth);
        state.background.height.setRange(kMinHeight, kMaxHeight / (*targetAspectRatio));
    }
    else
    {
        state.background.width.setRange(kMinWidth, kMaxWidth);
        state.background.height.setRange(kMinHeight, kMaxHeight);
    }

    return state;
}

State resetBackgroundParameters(State state)
{
    state.background = {};
    state = updateBackgroundLimits(std::move(state));
    state.background.width.value = state.background.width.min;
    state.background.height.value = state.background.height.min;

    state.background.opacityPercent = kDefaultBackgroundOpacityPercent;
    return state;
}

State applyRecommendedBackgroundSize(State state)
{
    const auto targetAspectRatio = bestAspectRatioForCells(state);
    NX_ASSERT(targetAspectRatio);
    if (!targetAspectRatio)
        return state;

    // Limit w*h <= recommended area; minor variations are allowed, e.g. 17*6 ~~= 100.
    const qreal height = qSqrt(qnGlobals->layoutBackgroundRecommendedArea() / (*targetAspectRatio));
    const qreal width = height * (*targetAspectRatio);
    state.background.width.setValue(width);
    state.background.height.setValue(height);

    return state;
}

//TODO: #GDM Code duplication.
// Aspect ratio of the current screen.
QnAspectRatio screenAspectRatio()
{
    const auto screen = qApp->desktop()->screenGeometry();
    return QnAspectRatio(screen.size());
}

int findFreeLogicalId(const QnLayoutResourcePtr& layout)
{
    if (!layout->resourcePool())
        return 1;

    auto layouts = layout->resourcePool()->getResources<QnLayoutResource>();
    std::set<int> usedValues;
    for (const auto& other: layouts)
    {
        if (other == layout)
            continue;
        const auto id = other->logicalId();
        if (id > 0)
            usedValues.insert(id);
    }

    int previousValue = 0;
    for (auto value: usedValues)
    {
        if (value > previousValue + 1)
            break;
        previousValue = value;
    }
    return previousValue + 1;
}

static std::atomic_bool tracingEnabled(false);

void trace(const State& state, const char* message)
{
    if (tracingEnabled)
    {
        NX_DEBUG(typeid(LayoutSettingsDialogStateReducer), message);
        NX_DEBUG(typeid(LayoutSettingsDialogStateReducer), state);
    }
}

} // namespace

bool LayoutSettingsDialogStateReducer::isTracingEnabled()
{
    return tracingEnabled;
}

void LayoutSettingsDialogStateReducer::setTracingEnabled(bool value)
{
    tracingEnabled = value;
}

State LayoutSettingsDialogStateReducer::loadLayout(State state, const QnLayoutResourcePtr& layout)
{
    state.locked = layout->locked();
    state.logicalId = layout->logicalId();
    state.reservedLogicalId = findFreeLogicalId(layout);
    state.isLocalFile = layout->isFile();

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
        state.cellAspectRatio = qnGlobals->defaultLayoutCellAspectRatio();
    }

    state = resetBackgroundParameters(std::move(state));
    state.background.filename = layout->backgroundImageFilename();
    state.background.width.setValue(layout->backgroundSize().width());
    state.background.height.setValue(layout->backgroundSize().height());
    state.background.opacityPercent = boundOpacityPercent(int(layout->backgroundOpacity() * 100));

    return state;
}

State LayoutSettingsDialogStateReducer::setLocked(State state, bool value)
{
    state.locked = value;
    return state;
}

State LayoutSettingsDialogStateReducer::setLogicalId(State state, int value)
{
    state.logicalId = value;
    return state;
}

State LayoutSettingsDialogStateReducer::resetLogicalId(State state)
{
    state.logicalId = 0;
    return state;
}

State LayoutSettingsDialogStateReducer::generateLogicalId(State state)
{
    state.logicalId = state.reservedLogicalId;
    return state;
}

State LayoutSettingsDialogStateReducer::setFixedSizeEnabled(State state, bool value)
{
    state.fixedSizeEnabled = value;
    if (value && state.fixedSize.isEmpty())
        state.fixedSize = {1, 1};
    return state;
}

State LayoutSettingsDialogStateReducer::setFixedSizeWidth(State state, int value)
{
    state.fixedSize.setWidth(value);
    return state;
}

State LayoutSettingsDialogStateReducer::setFixedSizeHeight(State state, int value)
{
    state.fixedSize.setHeight(value);
    return state;
}

State LayoutSettingsDialogStateReducer::setBackgroundImageError(State state, const QString& errorText)
{
    state.background.status = BackgroundImageStatus::error;
    state.background.errorText = errorText;
    return state;
}

State LayoutSettingsDialogStateReducer::clearBackgroundImage(State state)
{
    state = resetBackgroundParameters(std::move(state));
    return state;
}

State LayoutSettingsDialogStateReducer::setBackgroundImageOpacityPercent(State state, int value)
{
    state.background.opacityPercent = boundOpacityPercent(value);
    return state;
}

State LayoutSettingsDialogStateReducer::setBackgroundImageWidth(State state, int value)
{
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
    state.background.cropToMonitorAspectRatio = value;
    state = updateBackgroundLimits(std::move(state));
    if (!cellsAreBestAspected(state))
        state = applyRecommendedBackgroundSize(std::move(state));
    return state;
}

State LayoutSettingsDialogStateReducer::setKeepImageAspectRatio(State state, bool value)
{
    state.background.keepImageAspectRatio = value;
    state = updateBackgroundLimits(std::move(state));
    if (!cellsAreBestAspected(state))
        state = applyRecommendedBackgroundSize(std::move(state));
    return state;
}

State LayoutSettingsDialogStateReducer::startDownloading(State state, const QString& targetPath)
{
    NX_ASSERT(state.background.canStartDownloading());
    state.background.imageSourcePath = targetPath;
    state.background.status = BackgroundImageStatus::downloading;
    return state;
}

State LayoutSettingsDialogStateReducer::imageDownloaded(State state)
{
    NX_ASSERT(state.background.status == BackgroundImageStatus::downloading);
    state.background.status = BackgroundImageStatus::loading;
    return state;
}

State LayoutSettingsDialogStateReducer::imageSelected(State state, const QString& filename)
{
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
    // We can re-upload existing image if it was cropped.
    NX_ASSERT(state.background.status == BackgroundImageStatus::loaded
        || state.background.status == BackgroundImageStatus::newImageLoaded);
    state.background.status = BackgroundImageStatus::newImageUploading;
    return state;
}

State LayoutSettingsDialogStateReducer::imageUploaded(State state, const QString& filename)
{
    state.background.filename = filename;
    state.background.status = BackgroundImageStatus::newImageLoaded;
    return state;
}

State LayoutSettingsDialogStateReducer::setPreview(State state, const QImage& image)
{
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
    }
    else if (!qnSettings->layoutKeepAspectRatio())
    {
        // Keep false if previous accepted value was false.
        state.background.keepImageAspectRatio = false;
    }
    else
    {
        // Set to true if possible (current sizes will not be changed).
        state.background.keepImageAspectRatio = cellsAreBestAspected(state);
    }

    return state;
}

} // namespace nx::vms::client::desktop
