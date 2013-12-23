#ifndef QN_DRAG_PROCESS_HANDLER_H
#define QN_DRAG_PROCESS_HANDLER_H

#include <QtCore/QtGlobal> /* For Q_UNUSED. */

class DragProcessor;
class DragInfo;

/**
 * Interface for handling drag process state changes.
 */
class DragProcessHandler {
public:
    DragProcessHandler();
    virtual ~DragProcessHandler();

protected:
    /**
     * This function is called whenever drag process starts. It usually happens
     * when the user presses a mouse button.
     * 
     * It is guaranteed that <tt>finishDragProcess()</tt> will also be called.
     * 
     * \param info                      Information on the drag operation.
     */
    virtual void startDragProcess(DragInfo *info) { Q_UNUSED(info); }

    /**
     * This function is called whenever drag starts. It usually happens when
     * user moves the mouse far enough from the point where it was pressed,
     * or if he holds it pressed long enough.
     * 
     * Note that this function may not be called if the user releases the
     * mouse button before the drag could start.
     * 
     * It is guaranteed that if this function was called, then 
     * <tt>finishDrag()</tt> will also be called.
     * 
     * \param info                      Information on the drag operation.
     */
    virtual void startDrag(DragInfo *info) { Q_UNUSED(info); }

    /**
     * This function is called each time a mouse position changes while drag is
     * in progress.
     * 
     * \param info                      Information on the drag operation.
     */
    virtual void dragMove(DragInfo *info) { Q_UNUSED(info); }

    /**
     * This function is called whenever drag ends. It usually happens when the
     * user releases the mouse button that started the drag.
     * 
     * If <tt>startDrag()</tt> was called, then it is guaranteed that this 
     * function will also be called.
     * 
     * \param info                      Information on the drag operation.
     */
    virtual void finishDrag(DragInfo *info) { Q_UNUSED(info); }

    /**
     * This function is called whenever drag process ends. It usually happens 
     * right after the actual drag ends.
     * 
     * If <tt>startDragProcess()</tt> was called, then it is guaranteed that
     * this function will also be called.
     * 
     * \param info                      Information on the drag operation.
     */
    virtual void finishDragProcess(DragInfo *info) { Q_UNUSED(info); }

    /**
     * \returns                         Drag processor associated with this handler. 
     */
    DragProcessor *dragProcessor() const {
        return m_processor;
    }

private:
    friend class DragProcessor;

    DragProcessor *m_processor;
};

#endif // QN_DRAG_PROCESS_HANDLER_H
