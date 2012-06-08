#ifndef QN_RESIZING_INSTRUMENT_H
#define QN_RESIZING_INSTRUMENT_H

#include <QtCore/QWeakPointer>
#include <QtGui/QTransform>

#include "drag_processing_instrument.h"

class ConstrainedResizable;
class ResizeHoverInstrument;
class ResizingInstrument;

class ResizingInfo {
public:
    Qt::WindowFrameSection frameSection() const;

protected:
    friend class ResizingInstrument;

    ResizingInfo(ResizingInstrument *instrument): m_instrument(instrument) {}

private:
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

    ResizeHoverInstrument *resizeHoverInstrument() const {
        return m_resizeHoverInstrument;
    }

    int effectiveDistance() const {
        return m_effectiveDistance;
    }

    void setEffectiveDistance(int effectiveDistance) {
        m_effectiveDistance = effectiveDistance;
    }

    bool isEffective() const {
        return m_effective;
    }

    void setEffective(bool effective) {
        m_effective = effective;
    }

signals:
    void resizingProcessStarted(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);
    void resizingStarted(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);
    void resizing(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);
    void resizingFinished(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);
    void resizingProcessFinished(QGraphicsView *view, QGraphicsWidget *widget, const ResizingInfo &info);

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
    int m_effectiveDistance;
    bool m_effective;
    QRectF m_startGeometry;
    QTransform m_startTransform;
    bool m_resizingStartedEmitted;
    Qt::WindowFrameSection m_section;
    QWeakPointer<QGraphicsWidget> m_widget;
    ConstrainedResizable *m_resizable;
};

#endif // QN_RESIZING_INSTRUMENT_H
