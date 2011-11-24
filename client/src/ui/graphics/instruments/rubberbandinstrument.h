#ifndef QN_RUBBER_BAND_INSTRUMENT_H
#define QN_RUBBER_BAND_INSTRUMENT_H

#include <QScopedPointer>
#include <QPoint>
#include <QRect>
#include <QSet>
#include "dragprocessinginstrument.h"

class QGraphicsItem;

class RubberBandItem;

class RubberBandInstrument: public DragProcessingInstrument {
    Q_OBJECT;
public:
    RubberBandInstrument(QObject *parent);
    virtual ~RubberBandInstrument();

    qreal rubberBandZValue() const {
        return m_rubberBandZValue;
    }

    void setRubberBandZValue(qreal rubberBandZValue);

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

    virtual void startDrag() override;
    virtual void drag() override;
    virtual void finishDrag() override;

    static QSet<QGraphicsItem *> toSet(QList<QGraphicsItem *> items);

    RubberBandItem *rubberBand() const {
        return m_rubberBand.data();
    }

private slots:
    void at_scene_selectionChanged();

private:
    /* Rubber band item. */
    QWeakPointer<RubberBandItem> m_rubberBand;

    /** Set of items that were selected when rubber banding has started. */
    QSet<QGraphicsItem *> m_originallySelected;

    /** Whether the original selection needs to be restored. */
    bool m_protectSelection;

    /** Whether the control is currently inside selection change handler. */
    bool m_inSelectionChanged;

    /** Z value of the rubber band item. */
    qreal m_rubberBandZValue;
};

#endif // QN_RUBBER_BAND_INSTRUMENT_H
