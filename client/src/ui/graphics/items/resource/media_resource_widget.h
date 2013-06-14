#ifndef QN_MEDIA_RESOURCE_WIDGET_H
#define QN_MEDIA_RESOURCE_WIDGET_H

#include "resource_widget.h"

#include <QStaticText>

#include <core/datapacket/media_data_packet.h> /* For QnMetaDataV1Ptr. */ // TODO: #Elric FWD!
#include <core/resource/motion_window.h>
#include <core/resource/media_resource.h>

#include <client/client_globals.h>
#include "camera/resource_display.h" // TODO: #Elric FWD!
#include "utils/color_space/image_correction.h"

class QnResourceDisplay;
class QnResourceWidgetRenderer;

class QnMediaResourceWidget: public QnResourceWidget {
    Q_OBJECT
    typedef QnResourceWidget base_type;

public:
    static const Button MotionSearchButton = static_cast<Button>(0x08);
    static const Button PtzButton = static_cast<Button>(0x10);
    static const Button ZoomWindowButton = static_cast<Button>(0x20);
    static const Button HistogramButton = static_cast<Button>(0x40);
#define MotionSearchButton MotionSearchButton
#define PtzButton PtzButton
#define ZoomWindowButton ZoomWindowButton
#define HistogramButton HistogramButton

    QnMediaResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);
    virtual ~QnMediaResourceWidget();

    /**
     * \returns                         Resource associated with this widget.
     */
    QnMediaResourcePtr resource() const;

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

    /**
     * \returns                         Current motion selection regions.
     */
    const QList<QRegion> &motionSelection() const;

    bool addToMotionSensitivity(const QRect &gridRect, int sensitivity);

    bool setMotionSensitivityFilled(const QPoint &gridPos, int sensitivity);

    void clearMotionSensitivity();

    const QList<QnMotionRegion> &motionSensitivity() const;

    bool isMotionSensitivityEmpty() const;

    ImageCorrectionParams contrastParams() const;

signals:
    void motionSelectionChanged();
    void displayChanged();
public slots:
    void setContrastParams(const ImageCorrectionParams& params);
protected:
    virtual int helpTopicAt(const QPointF &pos) const override;

    virtual void channelLayoutChangedNotify() override;
    virtual void channelScreenSizeChangedNotify() override;
    virtual void optionsChangedNotify(Options changedFlags) override;

    virtual QString calculateInfoText() const override;
    virtual Buttons calculateButtonsVisibility() const override;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) override;
    virtual void paintChannelForeground(QPainter *painter, int channel, const QRectF &rect) override;
    void paintMotionSensitivityIndicators(QPainter *painter, int channel, const QRectF &rect, const QnMotionRegion &region);
    void paintMotionGrid(QPainter *painter, int channel, const QRectF &rect, const QnMetaDataV1Ptr &motion);
    void paintMotionSensitivity(QPainter *painter, int channel, const QRectF &rect);
    void paintFilledRegionPath(QPainter *painter, const QRectF &rect, const QPainterPath &path, const QColor &color, const QColor &penColor);

    void ensureMotionSensitivity() const;
    void invalidateMotionSensitivity();

    void ensureBinaryMotionMask() const;
    void invalidateBinaryMotionMask();

    void ensureMotionSelectionCache();
    void invalidateMotionSelectionCache();

    QSize motionGridSize() const;
    QPoint channelGridOffset(int channel) const;

    Q_SIGNAL void updateInfoTextLater();
private slots:
    void at_resource_resourceChanged();
    void at_searchButton_toggled(bool checked);
    void at_ptzButton_toggled(bool checked);
    void at_zoomWindowButton_toggled(bool checked);
    void at_histogramButton_toggled(bool checked);
    void at_camDisplay_liveChanged();
    void at_renderWatcher_displayingChanged(QnResourceWidget *widget);
private:
    void setDisplay(const QnResourceDisplayPtr &display);

    Q_SLOT void updateDisplay();
    Q_SLOT void updateAspectRatio();
    Q_SLOT void updateIconButton();
    Q_SLOT void updateRendererEnabled();

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
};

#endif // QN_MEDIA_RESOURCE_WIDGET_H
