#pragma once

#include "drag_processing_instrument.h"

class QnMediaResourceWidget;
class SelectionItem;

class MotionSelectionInstrument: public DragProcessingInstrument
{
    Q_OBJECT
    using base_type = DragProcessingInstrument;

public:
    explicit MotionSelectionInstrument(QObject* parent = nullptr);
    virtual ~MotionSelectionInstrument() override;

    void setPen(const QPen& pen);
    QPen pen() const;

    void setBrush(const QBrush& brush);
    QBrush brush() const;

    /**
     * \param selectionModifiers        Keyboard modifiers that must be pressed for the
     *                                  selection process to start. Defaults to 0.
     */
    void setSelectionModifiers(Qt::KeyboardModifiers selectionModifiers);
    Qt::KeyboardModifiers selectionModifiers() const;

    /**
     * \param multiSelectionModifiers   Additional keyboard modifiers that must be
     *                                  pressed for the new selection to be added
     *                                  to existing selection. Defalts to <tt>Qn::ControlModifier</tt>.
     */
    void setMultiSelectionModifiers(Qt::KeyboardModifiers multiSelectionModifiers);
    Qt::KeyboardModifiers multiSelectionModifiers() const;

signals:
    void selectionProcessStarted(QGraphicsView* view, QnMediaResourceWidget* widget);
    void selectionStarted(QGraphicsView* view, QnMediaResourceWidget* widget);
    void motionRegionCleared(QGraphicsView* view, QnMediaResourceWidget* widget);
    void motionRegionSelected(QGraphicsView* view, QnMediaResourceWidget* widget, const QRect& rect);
    void selectionFinished(QGraphicsView* view, QnMediaResourceWidget* widget);
    void selectionProcessFinished(QGraphicsView* view, QnMediaResourceWidget* widget);

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeDisabledNotify() override;
    virtual void aboutToBeUninstalledNotify() override;

    virtual bool mousePressEvent(QWidget* viewport, QMouseEvent* event) override;
    virtual bool mouseMoveEvent(QWidget* viewport, QMouseEvent* event) override;
    virtual bool mouseReleaseEvent(QWidget* viewport, QMouseEvent* event) override;
    virtual bool paintEvent(QWidget* viewport, QPaintEvent* event) override;

    virtual void startDragProcess(DragInfo* info) override;
    virtual void startDrag(DragInfo* info) override;
    virtual void dragMove(DragInfo* info) override;
    virtual void finishDrag(DragInfo* info) override;
    virtual void finishDragProcess(DragInfo* info) override;

    SelectionItem* selectionItem() const;

    QnMediaResourceWidget* target() const;

    Qt::KeyboardModifiers selectionModifiers(QnMediaResourceWidget* target) const;

    void ensureSelectionItem();

private:
    void updateWidgetUnderCursor(QWidget* viewport, QMouseEvent* event);
    void updateCursor();
    void setWidget(QnMediaResourceWidget* widget);
    void setItemUnderMouse(QGraphicsWidget* item);

    void updateButtonUnderCursor(QWidget* viewport, QMouseEvent* event);

private:
    QBrush m_brush;
    QPen m_pen;
    QPointer<SelectionItem> m_selectionItem;
    QPointer<QnMediaResourceWidget> m_widget;
    QPointer<QGraphicsWidget> m_itemUnderMouse;
    QPointer<QGraphicsWidget> m_buttonUnderMouse;
    bool m_selectionStartedEmitted = false;
    Qt::KeyboardModifiers m_selectionModifiers;
    Qt::KeyboardModifiers m_multiSelectionModifiers;
    QRect m_gridRect;
};

/**
 * Name of the property to set on a <tt>QnResourceWidget</tt> to change
 * the keyboard modifiers that must be pressed to activate the
 * motion selection instrument.
 *
 * If not set, default modifiers set for the motion selection instrument
 * will be used.
 */
#define MotionSelectionModifiers _id("_qn_motionSelectionModifiers")

/**
 * Name of the property to set on a graphics item to block motion selection.
 */
#define BlockMotionSelection _id("_qn_blockMotionSelection")
