#ifndef QN_MEDIA_RESOURCE_WIDGET_H
#define QN_MEDIA_RESOURCE_WIDGET_H

#include "resource_widget.h"

#include <QtGui/QStaticText>

#include <core/resource/resource_fwd.h>

#include <core/datapacket/media_data_packet.h> /* For QnMetaDataV1Ptr. */ // TODO: #Elric FWD!
#include <core/resource/motion_window.h>

#include <core/ptz/ptz_fwd.h>
#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>

#include <client/client_globals.h>
#include <camera/resource_display.h> // TODO: #Elric FWD!
#include <utils/color_space/image_correction.h>

class QnResourceDisplay;
class QnResourceWidgetRenderer;
class QnFisheyeHomePtzController;

class QnMediaResourceWidget: public QnResourceWidget {
    Q_OBJECT
    typedef QnResourceWidget base_type;

public:
    static const Button ScreenshotButton    = static_cast<Button>(0x008);
    static const Button MotionSearchButton  = static_cast<Button>(0x010);
    static const Button PtzButton           = static_cast<Button>(0x020);
    static const Button FishEyeButton       = static_cast<Button>(0x040);
    static const Button ZoomWindowButton    = static_cast<Button>(0x080);
    static const Button EnhancementButton   = static_cast<Button>(0x100);
    static const Button DbgScreenshotButton = static_cast<Button>(0x200);
#define ScreenshotButton ScreenshotButton
#define MotionSearchButton MotionSearchButton
#define PtzButton PtzButton
#define FishEyeButton FishEyeButton
#define ZoomWindowButton ZoomWindowButton
#define EnhancementButton EnhancementButton
#define DbgScreenshotButton DbgScreenshotButton

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

signals:
    void motionSelectionChanged();
    void displayChanged();
    void fisheyeChanged();
    void dewarpingParamsChanged();

protected:
    virtual int helpTopicAt(const QPointF &pos) const override;

    virtual void channelLayoutChangedNotify() override;
    virtual void channelScreenSizeChangedNotify() override;
    virtual void optionsChangedNotify(Options changedFlags) override;

    virtual QString calculateInfoText() const override;
    virtual QString calculateTitleText() const override;
    virtual Buttons calculateButtonsVisibility() const override;
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

    virtual void createCustomOverlays() override;
private slots:
    void at_resource_resourceChanged();
    void at_resource_propertyChanged(const QnResourcePtr &resource, const QString &key);
    void at_screenshotButton_clicked();
    void at_searchButton_toggled(bool checked);
    void at_ptzButton_toggled(bool checked);
    void at_fishEyeButton_toggled(bool checked);
    void at_zoomWindowButton_toggled(bool checked);
    void at_histogramButton_toggled(bool checked);
    void at_camDisplay_liveChanged();
    void at_statusOverlayWidget_diagnosticsRequested();
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
    mutable QList<__m128i *> m_binaryMotionMask;

    /** Whether motion mask binary data is valid. */
    mutable bool m_binaryMotionMaskValid;

    /** Whether motion selection cached paths are valid. */
    mutable bool m_motionSelectionCacheValid;

    QStaticText m_sensStaticText[10];

    QnPtzControllerPtr m_ptzController;
    QnFisheyeHomePtzController *m_homePtzController;

    QnMediaDewarpingParams m_dewarpingParams;
};

Q_DECLARE_METATYPE(QnMediaResourceWidget *)

#endif // QN_MEDIA_RESOURCE_WIDGET_H
