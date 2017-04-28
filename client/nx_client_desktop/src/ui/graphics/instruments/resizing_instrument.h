#pragma once

#include <QtGui/QTransform>

#include "drag_processing_instrument.h"

class ConstrainedGeometrically;
class ResizeHoverInstrument;
class ResizingInstrument;

class ResizingInfo
{
public:
    Qt::WindowFrameSection frameSection() const;

    QPoint mouseScreenPos() const;
    QPoint mouseViewportPos() const;
    QPointF mouseScenePos() const;

protected:
    friend class ResizingInstrument;

    ResizingInfo(DragInfo* info, ResizingInstrument* instrument):
        m_info(info),
        m_instrument(instrument)
    {}

private:
    DragInfo* m_info;
    ResizingInstrument* m_instrument;
};

typedef QPointer<QGraphicsWidget> QGraphicsWidgetPtr;
typedef QList<QGraphicsWidgetPtr> WidgetsList;


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
class ResizingInstrument: public DragProcessingInstrument
{
    Q_OBJECT

    using base_type = DragProcessingInstrument;

public:
    ResizingInstrument(QObject* parent = nullptr);
    virtual ~ResizingInstrument();

    qreal innerEffectRadius() const;
    void setInnerEffectRadius(qreal effectRadius);

    qreal outerEffectRadius() const;
    void setOuterEffectRadius(qreal effectRadius);

    void rehandle();

signals:
    void resizingProcessStarted(QGraphicsView* view, QGraphicsWidget* widget, ResizingInfo* info);
    void resizingStarted(QGraphicsView* view, QGraphicsWidget* widget, ResizingInfo* info);
    void resizing(QGraphicsView* view, QGraphicsWidget* widget, ResizingInfo* info);
    void resizingFinished(QGraphicsView* view, QGraphicsWidget* widget, ResizingInfo* info);
    void resizingProcessFinished(QGraphicsView* view, QGraphicsWidget* widget, ResizingInfo* info);

protected:
    virtual bool mousePressEvent(QWidget* viewport, QMouseEvent* event) override;
    virtual bool mouseMoveEvent(QWidget* viewport, QMouseEvent* event) override;

    virtual void startDragProcess(DragInfo* info) override;
    virtual void startDrag(DragInfo* info) override;
    virtual void dragMove(DragInfo* info) override;
    virtual void finishDrag(DragInfo* info) override;
    virtual void finishDragProcess(DragInfo* info) override;

private:
    void getWidgetAndFrameSection(
        QWidget* viewport,
        const QPoint& pos,
        Qt::WindowFrameSection& section,
        QGraphicsWidget*& widget,
        QPoint& correctedPos) const;

    QGraphicsWidget* resizableWidgetAtPos(
        QGraphicsView* view,
        const QPoint& pos
    ) const;

    Qt::WindowFrameSection queryFrameSection(
        QGraphicsView* view,
        QGraphicsWidget* widget,
        const QPoint& pos,
        qreal effectRadius) const;

    WidgetsList getAffectedWidgets(QWidget* viewport, const QPoint& pos) const;

private:
    friend class ResizingInfo;

    qreal m_innerEffectRadius;
    qreal m_outerEffectRadius;

    QPointF m_startPinPoint;
    QSizeF m_startSize;
    QTransform m_startTransform;
    bool m_resizingStartedEmitted;
    Qt::WindowFrameSection m_section;
    QGraphicsWidgetPtr m_widget;
    ConstrainedGeometrically* m_constrained;

    /** List of widgets, for which we have changed cursor. */
    QList<QGraphicsWidgetPtr> m_affectedWidgets;
};
