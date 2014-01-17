#ifndef QN_RESIZING_INSTRUMENT_H
#define QN_RESIZING_INSTRUMENT_H


#include <QtGui/QTransform>

#include "drag_processing_instrument.h"

class ConstrainedGeometrically;
class ResizeHoverInstrument;
class ResizingInstrument;

class ResizingInfo {
public:
    Qt::WindowFrameSection frameSection() const;

    QPoint mouseScreenPos() const;
    QPoint mouseViewportPos() const;
    QPointF mouseScenePos() const;

protected:
    friend class ResizingInstrument;

    ResizingInfo(DragInfo *info, ResizingInstrument *instrument): m_info(info), m_instrument(instrument) {}

private:
    DragInfo *m_info;
    ResizingInstrument *m_instrument;
};


/**
 * This instrument implements resizing of QGraphicsWidget. 
 * Unlike default resizing algorithm, it allows resizing to non-integer sizes.
 * Size hints are currently not supported.
 * 
 * This instrument implements the following resizing process:
 * <ol>
 * <li>User presses a mouse button on a widget's frame.</li>
 * <li>User moves a mouse pointer several pixels away intending to resize a widget.</li>
 * <li>Resizing starts. </li>
 * <li>User resizes a widget and releases the mouse button.</li>
 * <li>Resizing ends. </li>
 * </ol>
 *
 * Provided signals can be used to track what is happening.
 *
 * Note that resizing process may finish before the actual resizing starts if the user
 * releases the mouse button before moving the mouse pointer several pixels away.
 * In this case resizingStarted() signal will not be emitted.
 */
class ResizingInstrument: public DragProcessingInstrument {
    Q_OBJECT;

    typedef DragProcessingInstrument base_type;
public:
    ResizingInstrument(QObject *parent = NULL);
    virtual ~ResizingInstrument();

    Instrument *resizeHoverInstrument() const;

    qreal effectRadius() const {
        return m_effectRadius;
    }

    void setEffectRadius(qreal effectRadius) {
        m_effectRadius = effectRadius;
    }

    void rehandle();

signals:
    void resizingProcessStarted(QGraphicsView *view, QGraphicsWidget *widget, ResizingInfo *info);
    void resizingStarted(QGraphicsView *view, QGraphicsWidget *widget, ResizingInfo *info);
    void resizing(QGraphicsView *view, QGraphicsWidget *widget, ResizingInfo *info);
    void resizingFinished(QGraphicsView *view, QGraphicsWidget *widget, ResizingInfo *info);
    void resizingProcessFinished(QGraphicsView *view, QGraphicsWidget *widget, ResizingInfo *info);

protected:
    virtual void enabledNotify() override;
    virtual void aboutToBeDisabledNotify() override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private:
    friend class ResizingInfo;

    ResizeHoverInstrument *m_resizeHoverInstrument;
    qreal m_effectRadius;
    QPointF m_startPinPoint;
    QSizeF m_startSize;
    QTransform m_startTransform;
    bool m_resizingStartedEmitted;
    Qt::WindowFrameSection m_section;
    QPointer<QGraphicsWidget> m_widget;
    ConstrainedGeometrically *m_constrained;
};

#endif // QN_RESIZING_INSTRUMENT_H
