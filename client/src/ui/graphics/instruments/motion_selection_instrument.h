#ifndef QN_MOTION_SELECTION_INSTRUMENT_H
#define QN_MOTION_SELECTION_INSTRUMENT_H

#include "drag_processing_instrument.h"

#include <QtCore/QWeakPointer>

class QnMediaResourceWidget;
class MotionSelectionItem;

class MotionSelectionInstrument: public DragProcessingInstrument {
    Q_OBJECT;

    typedef DragProcessingInstrument base_type;

public:
    enum ColorRole {
        Base,       /**< Color of the selection rect. */
        Border,     /**< Color of the selection rect's border. */
        MouseBorder,/**< Color of the mouse rect's border. */
        RoleCount
    };

    MotionSelectionInstrument(QObject *parent = NULL);
    virtual ~MotionSelectionInstrument();

    void setColor(ColorRole role, const QColor &color);
    QColor color(ColorRole role) const;

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
    void selectionProcessStarted(QGraphicsView *view, QnMediaResourceWidget *widget);
    void selectionStarted(QGraphicsView *view, QnMediaResourceWidget *widget);
    void motionRegionCleared(QGraphicsView *view, QnMediaResourceWidget *widget);
    void motionRegionSelected(QGraphicsView *view, QnMediaResourceWidget *widget, const QRect &rect);
    void selectionFinished(QGraphicsView *view, QnMediaResourceWidget *widget);
    void selectionProcessFinished(QGraphicsView *view, QnMediaResourceWidget *widget);

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeDisabledNotify() override;
    virtual void aboutToBeUninstalledNotify() override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

    MotionSelectionItem *selectionItem() const {
        return m_selectionItem.data();
    }

    QnMediaResourceWidget *target() const {
        return m_target.data();
    }

    Qt::KeyboardModifiers selectionModifiers(QnMediaResourceWidget *target) const;

    void ensureSelectionItem();

private:
    QColor m_colors[RoleCount];
    QWeakPointer<MotionSelectionItem> m_selectionItem;
    QWeakPointer<QnMediaResourceWidget> m_target;
    bool m_selectionStartedEmitted;
    bool m_isClick;
    bool m_clearingBlocked;
    Qt::KeyboardModifiers m_selectionModifiers;
    Qt::KeyboardModifiers m_multiSelectionModifiers;
    QRect m_gridRect;
};


namespace Qn { namespace {
    /**
     * Name of the property to set on a <tt>QnResourceWidget</tt> to change
     * the keyboard modifiers that must be pressed to activate the
     * motion selection instrument.
     * 
     * If not set, default modifiers set for the motion selection instrument
     * will be used.
     */
    const char *MotionSelectionModifiers = "_qn_motionSelectionModifiers";

    /**
     * Name of the property to set on a graphics item to stop it from
     * blocking motion selection.
     */
    const char *NoBlockMotionSelection = "_qn_noBlockMotionSelection";

#define MotionSelectionModifiers MotionSelectionModifiers
#define NoBlockMotionSelection NoBlockMotionSelection
}}


#endif // QN_MOTION_SELECTION_INSTRUMENT_H
