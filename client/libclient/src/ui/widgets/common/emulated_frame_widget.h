#ifndef QN_EMULATED_FRAME_WIDGET_H
#define QN_EMULATED_FRAME_WIDGET_H

#include <QtCore/QBasicTimer>
#include <QtWidgets/QWidget>

#include <ui/processors/drag_processor.h>
#include <ui/processors/drag_process_handler.h>

class QnEmulatedFrameWidget: public QWidget, protected DragProcessHandler {
    Q_OBJECT;

    typedef QWidget base_type;

public:
    QnEmulatedFrameWidget(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnEmulatedFrameWidget();

    void showFullScreen();
    void showNormal();

protected:
    /**
     * This function is to be overridden in derived class to provide information
     * on window frame sections.
     *
     * \param pos                       Position in widget coordinates.
     * \returns                         Window frame section at the given position.
     */
    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPoint &pos) const = 0;

    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void timerEvent(QTimerEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private:
    void updateCursor();
    void updateCursor(const QPoint &mousePos);

private:
    friend class QnEmulatedFrameWidgetDragProcessHandler;

    DragProcessor *m_dragProcessor;
    Qt::WindowFrameSection m_section;
    QPoint m_startPinPoint;
    QPoint m_startPosition;
    QSize m_startSize;

    QPoint m_lastMousePos;
    QBasicTimer m_timer;
};


#endif // QN_EMULATED_FRAME_WIDGET_H
