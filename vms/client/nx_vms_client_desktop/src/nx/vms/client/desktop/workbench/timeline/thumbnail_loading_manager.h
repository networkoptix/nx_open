// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSize>

#include <recording/time_period.h>
#include <utils/common/aspect_ratio.h>

#include "thumbnail.h"

class QnTimeSlider;
class QnMediaResourceWidget;
class QnTimePeriodList;

namespace nx::vms::client::desktop::workbench::timeline {

class ThumbnailLoader;

/**
 * Creates and manages ThumbnailLoader objects to perform loading of thumbnails for Thumbnail Panel
 * and Live Preview tooltip:
 * - m_panelLoader is used to preload thumbnails for Thumbnail Panel.
 * - m_livePreviewLoader is used to load individual Live Preview thumbnails requested by hovering
 *   mouse over timeline.
 */
class ThumbnailLoadingManager: public QObject
{
    Q_OBJECT

public:
    /**
     * Static size parameters used for Thumbnail Panel and Live Preview thumbnails. They should
     * be used from here by other timeline thumbnail related classes instead of duplicating.
     */
    static constexpr QSize kDefaultSize = {128, 72};
    static constexpr QSize kMinimumSize = {96, 54};
    static constexpr int kMaximumHeight = 196;

    /**
     * @param resourceWidget Pointer to a QnMediaResourceWidget object for which thumbnails
     *     will be generated.
     * @param slider Pointer to singleton QnTimeSlider object.
     */
    ThumbnailLoadingManager(QnMediaResourceWidget* resourceWidget, QnTimeSlider* slider);
    virtual ~ThumbnailLoadingManager() override;

    QnMediaResourceWidget* resourceWidget() const;

    /**
     * Should be called to clear previous and start loading new panel thumbnails when any of the
     * following timeline parameters changed: Thumbnail Panel preview bounding size, msecsPerPixel,
     * Thumbnail Panel visibility. See also setPanelWindowPeriod() below.
     * @param boundingHeight New Thumbnail Panel bounding height.
     * @param timeStep New time step between adjacent Thumbnail Panel thumbnails. Ignored if
     *     Thumbnail Panel is not visible.
     * @param panelWindowPeriod Archive time period corresponding for Thumbnail Panel rect.
     */
    void refresh(int boundingHeight,
        std::chrono::milliseconds timeStep,
        const QnTimePeriod& panelWindowPeriod);

    /**
     * @return Thumbnail aspect ratio, same as aspect ratio of the corresponding media resource
     *     widget.
     */
    QnAspectRatio aspectRatio() const;

    /**
     * @return Thumbnail rotation, same as rotation of the corresponding media resource widget,
     *     rounded to the nearest quadrant.
     */
    qreal rotation() const;

    /**
     * @return Bounding height of the Thumbnail Panel previously set by refresh() call.
     */
    int panelBoundingHeight() const;

    /**
     * @return Actual size of single Thumbnail Panel thumbnail.
     */
    QSize panelThumbnailSize() const;

    /**
     * @return Time step between desired timestamps of two adjacent Thumbnail Panel thumbnails.
     *     Updates on refresh() call only.
     */
    std::chrono::milliseconds panelTimeStep() const;

    /**
     * @return Actual size of single Live Preview thumbnail.
     */
    QSize previewThumbnailSize() const;

    /**
     * @return Time step between timestamps of two adjacent Live Preview thumbnails. Updates on
     *     refresh() call only.
     */
    std::chrono::milliseconds previewTimeStep() const;

    /**
     * @return Visibility of Thumbnail Panel as it was observed at previous refresh() call.
     */
    bool wasPanelVisibleAtLastRefresh() const;

    /**
     * Sets new time period for thumbnails panel. Should be used only if an event caused
     * period change changed none from the following timeline parameters: Thumbnail Panel
     * bounding size, msecsPerPixel, Thumbnail Panel visibility. Otherwise, call refresh() instead.
     * @param panelWindowPeriod Archive time period corresponding for Thumbnail Panel rect.
     */
    void setPanelWindowPeriod(const QnTimePeriod& panelWindowPeriod);

    /**
     * @return All previously loaded Thumbnail Panel thumbnails. Keys are thumbnail timestamps
     *     aligned by panelTimeStep() grid.
     */
    QVector<ThumbnailPtr> panelThumbnails() const;

    /**
     * Returns previously loaded thumbnail for Live Preview, or starts loading new one if absent.
     * @param desiredTime Time of already loaded thumbnail to get at. All thumbnails are aligned
     *     by Live Preview time step grid, so desiredTime will be rounded to nearest aligned value
     *     before lookup or loading (if absent). Aligned value will be written by outThumbnailTime
     *     pointer.
     * @param[out] outThumbnailTime If not nullptr, desiredTime aligned by time step grid will be
     *     written.
     * @return Previously loaded thumbnail at desiredTime aligned by m_timeStep grid or nullptr
     *     if no such thumbnail loaded yet.
     */
    ThumbnailPtr previewThumbnailAt(std::chrono::milliseconds desiredTime,
        std::chrono::milliseconds* outThumbnailTime);

    void setRecordedTimePeriods(const QnTimePeriodList& recordedTimePeriods);

    /**
     * Whether to apply default rotation to Live Preview (default: false).
     */
    bool livePreviewPreRotated() const;
    void setLivePreviewPreRotated(bool value);

signals:

    /**
     * Signal is emitted whenever the thumbnail aspect ratio changes.
     */
    void aspectRatioChanged();

    /**
     * Signal is emitted whenever the thumbnail rotation changes.
     */
    void rotationChanged();

    /**
     * Emitted when every new thumbnail for Thumbnail Panel is loaded.
     * @param thumbnail Newly loaded thumbnail.
     */
    void panelThumbnailLoaded(ThumbnailPtr thumbnail);

    /**
     * Emitted when new thumbnail for Live Preview directly requested by previewThumbnailAt()
     * is loaded.
     * @param thumbnail Newly loaded thumbnail.
     */
    void individualPreviewThumbnailLoaded(ThumbnailPtr thumbnail);

    /**
     * Emitted when all previously loaded Thumbnail Panel thumbnails are cleared. This happens
     * when panel bounding size changed, or panel time step changed, or previously loaded
     * thumbnails occupied RAM over limit.
     */
    void panelThumbnailsInvalidated();

    /**
     * Emitted every time when (frame to thumbnail) scaling algorithm found new target thumbnail
     * size. Typical cases are loading first thumbnail for given QnMediaResource and changing
     * Thumbnail Panel height.
     */
    void previewTargetSizeChanged(QSize size);

private:
    void applyPanelWindowPeriod();

private:
    QnMediaResourceWidget* m_mediaResourceWidget;
    QnTimeSlider* const m_slider;
    std::unique_ptr<ThumbnailLoader> m_panelLoader;
    std::unique_ptr<ThumbnailLoader> m_livePreviewLoader;
    double m_rotation = 0.0;
    QnTimePeriod m_panelWindowPeriod;
    int m_panelBoundingHeight = 0;
    bool m_panelVisible;
    bool m_livePreviewPreRotated = false;
};

} // nx::vms::client::desktop::workbench::timeline
