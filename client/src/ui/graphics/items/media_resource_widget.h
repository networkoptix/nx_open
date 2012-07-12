#ifndef QN_MEDIA_RESOURCE_WIDGET_H
#define QN_MEDIA_RESOURCE_WIDGET_H

#include "resource_widget.h"

class QnResourceDisplay;

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

public signals:
    void motionSelectionChanged();

protected:
    int motionGridWidth() const;
    int motionGridHeight() const;
    void drawMotionSensitivity(QPainter *painter, const QRectF &rect, const QnMotionRegion& region, int channel);

    void ensureMotionMask();
    void invalidateMotionMask();
    void ensureMotionMaskBinData();
    void invalidateMotionMaskBinData();

    virtual void updateChannelScreenSize(const QSize &channelScreenSize) override;

private:
    /** Display. */
    QnResourceDisplay *m_display;

    /** Associated renderer. */
    QnResourceWidgetRenderer *m_renderer;

    /** Selected region for search-by-motion, in parrots. */
    QList<QRegion> m_motionSelection;

    /** Image region where motion is currently present, in parrots. */
    QList<QnMotionRegion> m_motionRegionList; // TODO: WHY THE HELL THIS ONE IS OF SIZE 4?????????? Find the one who did it and use your swordsmanship skillz on him.

    /** Whether the motion mask is valid. */
    bool m_motionMaskValid;

    /** Binary mask for the current motion region. */
    QVector<__m128i *> m_motionMaskBinData;

    /** Whether motion mask binary data is valid. */
    bool m_motionMaskBinDataValid;

};

#endif // QN_MEDIA_RESOURCE_WIDGET_H
