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
    static const Button MotionSearchButton = static_cast<Button>(0x8);
#define MotionSearchButton MotionSearchButton

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

signals:
    void motionSelectionChanged();

private slots:
    void at_renderer_sourceSizeChanged(const QSize &size);
    void at_resource_resourceChanged();
    void at_searchButton_toggled(bool checked);

protected:
    virtual void channelLayoutChangedNotify() override;
    virtual void channelScreenSizeChangedNotify() override;
    virtual void displayFlagsChangedNotify(DisplayFlags changedFlags) override;

    virtual QString calculateInfoText() const override;
    virtual Buttons calculateButtonsVisibility() const override;
    virtual Overlay calculateChannelOverlay(int channel) const override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual Qn::RenderStatus paintChannel(QPainter *painter, int channel, const QRectF &rect) override;
    void paintMotionSensitivityIndicators(QPainter *painter, int channel, const QRectF &rect, const QnMotionRegion &region);
    void paintMotionGrid(QPainter *painter, int channel, const QRectF &rect, const QnMetaDataV1Ptr &motion);
    void paintMotionSensitivity(QPainter *painter, int channel, const QRectF &rect);
    void paintFilledRegion(QPainter *painter, const QRectF &rect, const QRegion &selection, const QColor &color, const QColor &penColor);

    void ensureMotionSensitivity() const;
    void invalidateMotionSensitivity();

    void ensureBinaryMotionMask() const;
    void invalidateBinaryMotionMask();

    int motionGridWidth() const;
    int motionGridHeight() const;

    QPoint channelGridOffset(int channel) const;

    Q_SIGNAL void updateInfoTextLater();

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
    mutable QList<QnMotionRegion> m_motionSensitivity;

    /** Whether the motion sensitivity is valid. */
    mutable bool m_motionSensitivityValid;

    /** Binary mask for the current motion region. */
    mutable QList<__m128i *> m_binaryMotionMask;

    /** Whether motion mask binary data is valid. */
    mutable bool m_binaryMotionMaskValid;

    QnImageButtonWidget *m_searchButton;

    QStaticText m_sensStaticText[10];

};

#endif // QN_MEDIA_RESOURCE_WIDGET_H
