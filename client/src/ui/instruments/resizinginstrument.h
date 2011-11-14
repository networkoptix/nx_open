#ifndef QN_RESIZING_INSTRUMENT_H
#define QN_RESIZING_INSTRUMENT_H

#include <QWeakPointer>
#include "dragprocessinginstrument.h"

class ConstrainedResizable;

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
public:
    ResizingInstrument(QObject *parent = NULL);
    virtual ~ResizingInstrument();

signals:
    void resizingProcessStarted(QGraphicsView *view, QGraphicsWidget *widget);
    void resizingStarted(QGraphicsView *view, QGraphicsWidget *widget);
    void resizingFinished(QGraphicsView *view, QGraphicsWidget *widget);
    void resizingProcessFinished(QGraphicsView *view, QGraphicsWidget *widget);

protected:
    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

    virtual void startDragProcess() override;
    virtual void startDrag() override;
    virtual void drag() override;
    virtual void finishDrag() override;
    virtual void finishDragProcess() override;

private:
    QRectF m_startGeometry;
    bool m_resizingStartedEmitted;
    Qt::WindowFrameSection m_section;
    QWeakPointer<QGraphicsWidget> m_widget;
    ConstrainedResizable *m_resizable;
};

#endif // QN_RESIZING_INSTRUMENT_H
