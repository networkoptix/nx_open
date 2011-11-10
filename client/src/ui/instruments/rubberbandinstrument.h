#ifndef QN_RUBBER_BAND_INSTRUMENT_H
#define QN_RUBBER_BAND_INSTRUMENT_H

#include <QScopedPointer>
#include <QPoint>
#include <QRect>
#include <QSet>
#include "instrument.h"

class QGraphicsItem;

class RubberBandItem;

class RubberBandInstrument: public Instrument {
    Q_OBJECT;
public:
    RubberBandInstrument(QObject *parent);
    virtual ~RubberBandInstrument();

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

    static QSet<QGraphicsItem *> toSet(QList<QGraphicsItem *> items);

private slots:
    void at_scene_selectionChanged();

private:
    enum State {
        INITIAL,
        PREPAIRING,
        RUBBER_BANDING
    };

    /** Current state. */
    State m_state;

    /* Scene position of the last rubber band-related mouse press event. */
    QPointF m_mousePressScenePos;

    /** Widget position of the last rubber band-related mouse press event. */
    QPoint m_mousePressPos;

    /* Rubber band item. */
    QScopedPointer<RubberBandItem> m_rubberBand;

    /** Set of items that were selected when rubber banding has started. */
    QSet<QGraphicsItem *> m_originallySelected;

    /** Whether the original selection needs to be restored. */
    bool m_protectSelection;

    /** Whether the control is currently inside selection change handler. */
    bool m_inSelectionChanged;
};

#endif // QN_RUBBER_BAND_INSTRUMENT_H
