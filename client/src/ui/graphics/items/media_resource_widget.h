#ifndef QN_MEDIA_RESOURCE_WIDGET_H
#define QN_MEDIA_RESOURCE_WIDGET_H

#include "resource_widget.h"

#include <core/datapacket/mediadatapacket.h> /* For QnMetaDataV1Ptr. */
#include <core/resource/motion_window.h>

class QnResourceDisplay;
class QnResourceWidgetRenderer;


class QnMediaResourceWidget: public QnResourceWidget {
    Q_OBJECT;

    typedef QnResourceWidget base_type;

public:
    QnMediaResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);
    virtual ~QnMediaResourceWidget();

    /**
     * \returns                         Resource associated with this widget.
     */
    QnMediaResourcePtr resource() const;

    /**
     * \returns                         Display associated with this widget.
     */
    QnResourceDisplay *display() const {
        return m_display;
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
    QList<QRegion> motionSelection() const;

    void addToMotionRegion(int sensitivity, const QRect &rect, int channel);

    bool setMotionRegionSensitivity(int sensitivity, const QPoint &gridPos, int channel);

    void clearMotionRegions();

    const QList<QnMotionRegion> &motionRegionList();

    bool isMotionRegionsEmpty() const;

signals:
    void motionSelectionChanged();

public slots:
    void at_sourceSizeChanged(const QSize &size);

protected:
    virtual void contentLayoutChangedNotify() override;
    virtual void channelScreenSizeChangedNotify() override;

    virtual Qn::RenderStatus paintChannel(QPainter *painter, int channel, const QRectF &rect) override;

    void drawMotionSensitivity(QPainter *painter, const QRectF &rect, const QnMotionRegion& region, int channel);

    void drawMotionGrid(QPainter *painter, const QRectF &rect, const QnMetaDataV1Ptr &motion, int channel);

    void drawMotionMask(QPainter *painter, const QRectF& rect, int channel);

    void drawFilledRegion(QPainter *painter, const QRectF &rect, const QRegion &selection, const QColor& color, const QColor& penColor);

    void ensureMotionMask();
    void invalidateMotionMask();

    void ensureMotionMaskBinData();
    void invalidateMotionMaskBinData();

    int motionGridWidth() const;
    int motionGridHeight() const;

private:
    /** Media resource. */
    QnMediaResourcePtr m_resource;

    /** Display. */
    QnResourceDisplay *m_display;

    /** Associated renderer. */
    QnResourceWidgetRenderer *m_renderer;

    /** Selected region for search-by-motion, in parrots. */
    QList<QRegion> m_motionSelection;

    /** Image region where motion is currently present, in parrots. */
    QList<QnMotionRegion> m_motionSensitivity;

    /** Whether the motion sensitivity is valid. */
    bool m_motionSensitivityValid;

    /** Binary mask for the current motion region. */
    QList<__m128i *> m_binaryMotionMask;

    /** Whether motion mask binary data is valid. */
    bool m_binaryMotionMaskValid;


    QStaticText m_sensStaticText[10];

};

#endif // QN_MEDIA_RESOURCE_WIDGET_H
