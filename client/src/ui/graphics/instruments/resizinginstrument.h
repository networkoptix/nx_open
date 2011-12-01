#ifndef QN_RESIZING_INSTRUMENT_H
#define QN_RESIZING_INSTRUMENT_H

#include <QWeakPointer>
#include "dragprocessinginstrument.h"

class ConstrainedResizable;
class ResizeHoverInstrument;

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

    ResizeHoverInstrument *resizeHoverInstrument() const {
        return m_resizeHoverInstrument;
    }

    int effectiveDistance() const {
        return m_effectiveDistance;
    }

    void setEffectiveDistance(int effectiveDistance) {
        m_effectiveDistance = effectiveDistance;
    }

signals:
    void resizingProcessStarted(QGraphicsView *view, QGraphicsWidget *widget);
    void resizingStarted(QGraphicsView *view, QGraphicsWidget *widget);
    void resizingFinished(QGraphicsView *view, QGraphicsWidget *widget);
    void resizingProcessFinished(QGraphicsView *view, QGraphicsWidget *widget);

protected:
    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private:
    ResizeHoverInstrument *m_resizeHoverInstrument;
    int m_effectiveDistance;
    QRectF m_startGeometry;
    bool m_resizingStartedEmitted;
    Qt::WindowFrameSection m_section;
    QWeakPointer<QGraphicsWidget> m_widget;
    ConstrainedResizable *m_resizable;
};

#endif // QN_RESIZING_INSTRUMENT_H
