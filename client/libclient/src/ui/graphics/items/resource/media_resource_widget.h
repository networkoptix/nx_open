#ifndef QN_MEDIA_RESOURCE_WIDGET_H
#define QN_MEDIA_RESOURCE_WIDGET_H

#include "resource_widget.h"

#include <QtGui/QStaticText>

#include <camera/camera_bookmarks_manager_fwd.h>

#include <core/resource/resource_fwd.h>

struct QnMetaDataV1;
typedef std::shared_ptr<QnMetaDataV1> QnMetaDataV1Ptr;

#include <core/resource/motion_window.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <core/ptz/ptz_fwd.h>
#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>

#include <client/client_globals.h>
#include <camera/resource_display.h> //< TODO: #Elric FWD!
#include <utils/license_usage_helper.h>
#include <utils/color_space/image_correction.h>
#include <utils/media/sse_helper.h>

class QnResourceDisplay;
class QnResourceWidgetRenderer;
class QnFisheyeHomePtzController;
class QnCachingCameraDataLoader;
class QnIoModuleOverlayWidget;
class QnCompositeTextOverlay;

class QnMediaResourceWidget: public QnResourceWidget {
    Q_OBJECT
    typedef QnResourceWidget base_type;

public:
    QnMediaResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);
    virtual ~QnMediaResourceWidget();

    /**
     * \returns                         Resource associated with this widget.
     */
    const QnMediaResourcePtr &resource() const;

    /**
     * \returns                         Display associated with this widget.
     */
    QnResourceDisplayPtr display() const {
        return m_display;
    }

    QnResourceWidgetRenderer *renderer() const {
        return m_renderer;
    }

    /**
     * \param itemPos                   Point in item coordinates to map to grid coordinates.
     * \returns                         Coordinates of the motion cell that the given point belongs to.
     *                                  Note that motion grid is finite, so even if the
     *                                  passed coordinate lies outside the item boundary,
     *                                  returned joint will lie inside it.
     */
    QPoint mapToMotionGrid(const QPointF &itemPos);

    /**
     * \param gridPos                   Coordinate of the motion grid cell.
     * \returns                         Position in scene coordinates of the top left corner of the grid cell.
     */
    QPointF mapFromMotionGrid(const QPoint &gridPos);

    /**
     * \param gridRect                  Rectangle in grid coordinates to add to
     *                                  selected motion region of this widget.
     */
    void addToMotionSelection(const QRect &gridRect);

    /**
     * Clears this widget's motion selection region.
     */
    void clearMotionSelection();

    bool isMotionSelectionEmpty() const;

    void setMotionSelection(const QList<QRegion> &regions);

    /**
     * \returns                         Current motion selection regions.
     */
    const QList<QRegion> &motionSelection() const;

    bool addToMotionSensitivity(const QRect &gridRect, int sensitivity);

    bool setMotionSensitivityFilled(const QPoint &gridPos, int sensitivity);

    void clearMotionSensitivity();

    const QList<QnMotionRegion> &motionSensitivity() const;

    bool isMotionSensitivityEmpty() const;

    ImageCorrectionParams imageEnhancement() const;
    void setImageEnhancement(const ImageCorrectionParams &imageEnhancement);

    /**
     * This function returns a PTZ controller associated with this widget.
     * Note that this function never returns NULL.
     *
     * \returns                         PTZ controller associated with this widget.
     */
    QnPtzControllerPtr ptzController() const;

    QnMediaDewarpingParams dewarpingParams() const;
    void setDewarpingParams(const QnMediaDewarpingParams &params);

    virtual float visualAspectRatio() const;
    virtual float defaultVisualAspectRatio() const override;

    /** Check if the widget has video. It can be absent in I/O Module, for example. */
    bool hasVideo() const;

    QnCompositeTextOverlay *compositeTextOverlay();

signals:
    void motionSelectionChanged();
    void displayChanged();
    void fisheyeChanged();
    void dewarpingParamsChanged();
    void colorsChanged();
    void positionChanged(qint64 positionUtcMs);

protected:
    virtual int helpTopicAt(const QPointF &pos) const override;

    virtual void channelLayoutChangedNotify() override;
    virtual void channelScreenSizeChangedNotify() override;
    virtual void optionsChangedNotify(Options changedFlags) override;

    virtual QString calculateDetailsText() const override;
    virtual QString calculatePositionText() const override;
    virtual QString calculateTitleText() const override;
    virtual int calculateButtonsVisibility() const override;
    virtual QCursor calculateCursor() const override;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) override;
    virtual void paintChannelForeground(QPainter *painter, int channel, const QRectF &rect) override;
    void paintMotionSensitivityIndicators(QPainter *painter, int channel, const QRectF &rect, const QnMotionRegion &region);
    void paintMotionGrid(QPainter *painter, int channel, const QRectF &rect, const QnMetaDataV1Ptr &motion);
    void paintMotionSensitivity(QPainter *painter, int channel, const QRectF &rect);
    void paintFilledRegionPath(QPainter *painter, const QRectF &rect, const QPainterPath &path, const QColor &color, const QColor &penColor);

    void ensureMotionSensitivity() const;
    Q_SLOT void invalidateMotionSensitivity();

    void ensureBinaryMotionMask() const;
    void invalidateBinaryMotionMask();

    void ensureMotionSelectionCache();
    void invalidateMotionSelectionCache();

    QSize motionGridSize() const;
    QPoint channelGridOffset(int channel) const;

    Q_SIGNAL void updateInfoTextLater();

    void suspendHomePtzController();
    void resumeHomePtzController();

    virtual void updateHud(bool animate);

private slots:
    void at_resource_resourceChanged();
    void at_resource_propertyChanged(const QnResourcePtr &resource, const QString &key);
    void at_screenshotButton_clicked();
    void at_searchButton_toggled(bool checked);
    void at_ptzButton_toggled(bool checked);
    void at_fishEyeButton_toggled(bool checked);
    void at_zoomWindowButton_toggled(bool checked);
    void at_histogramButton_toggled(bool checked);
    void at_ioModuleButton_toggled(bool checked);
    void at_camDisplay_liveChanged();
    void at_statusOverlayWidget_diagnosticsRequested();
    void at_statusOverlayWidget_ioEnableRequested();
    void at_statusOverlayWidget_moreLicensesRequested();
    void at_renderWatcher_widgetChanged(QnResourceWidget *widget);
    void at_zoomRectChanged();
    void at_ptzController_changed(Qn::PtzDataFields fields);

    void at_item_imageEnhancementChanged();
    void at_videoLayoutChanged();
private:
    void setDisplay(const QnResourceDisplayPtr &display);
    void createButtons();

    Q_SLOT void updateDisplay();
    Q_SLOT void updateAspectRatio();
    Q_SLOT void updateIconButton();
    Q_SLOT void updateRendererEnabled();
    Q_SLOT void updateFisheye();
    Q_SLOT void updateDewarpingParams();
    Q_SLOT void updateCustomAspectRatio();
    Q_SLOT void updateIoModuleVisibility(bool animate);
    Q_SLOT void updateOverlayButton();

    void updateCompositeOverlayMode();

    qint64 getDisplayTimeUsec() const;
    qint64 getUtcCurrentTimeUsec() const;
    qint64 getUtcCurrentTimeMs() const;

    void updateCurrentUtcPosMs();

private:
    struct ResourceStates
    {
        bool isRealTimeSource;  /// Shows if resource is real-time source
        bool isOffline;         /// Shows if resource is offline. Not-real-time resource is alwasy online
        bool isUnauthorized;    /// Shows if resource is unauthorized. Not-real-time resource is alwasy online
        bool hasVideo;          /// Shows if resource has video
    };

    /// @brief Return resource states
    ResourceStates getResourceStates() const;

private:
    /** Media resource. */
    QnMediaResourcePtr m_resource;

    /** Camera resource. */
    QnVirtualCameraResourcePtr m_camera;

    /** Display. */
    QnResourceDisplayPtr m_display;

    /** Associated renderer. */
    QnResourceWidgetRenderer *m_renderer;

    /** Selected region for search-by-motion, in parrots. */
    QList<QRegion> m_motionSelection;

    /** Painter path cache for the list of selected regions. */
    QList<QPainterPath> m_motionSelectionPathCache;

    QVector<bool> m_paintedChannels;

    /** Image region where motion is currently present, in parrots. */
    mutable QList<QnMotionRegion> m_motionSensitivity;

    /** Whether the motion sensitivity is valid. */
    mutable bool m_motionSensitivityValid;

    /** Binary mask for the current motion region. */
    mutable QList<simd128i *> m_binaryMotionMask;

    /** Whether motion mask binary data is valid. */
    mutable bool m_binaryMotionMaskValid;

    /** Whether motion selection cached paths are valid. */
    mutable bool m_motionSelectionCacheValid;

    QStaticText m_sensStaticText[10];

    QnPtzControllerPtr m_ptzController;
    QnFisheyeHomePtzController *m_homePtzController;

    QnMediaDewarpingParams m_dewarpingParams;

    QnCompositeTextOverlay *m_compositeTextOverlay;

    QnIoModuleOverlayWidget *m_ioModuleOverlayWidget;
    bool m_ioCouldBeShown;

    typedef QScopedPointer<QnSingleCamLicenceStatusHelper> QnSingleCamLicenceStatusHelperPtr;
    QnSingleCamLicenceStatusHelperPtr m_ioLicenceStatusHelper;

    qint64 m_posUtcMs;
};

Q_DECLARE_METATYPE(QnMediaResourceWidget *)

#endif // QN_MEDIA_RESOURCE_WIDGET_H
