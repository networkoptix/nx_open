#ifndef QN_EMULATED_FRAME_WIDGET_H
#define QN_EMULATED_FRAME_WIDGET_H

#include <QtGui/QWidget>

#include <ui/processors/drag_processor.h>
#include <ui/processors/drag_process_handler.h>

class QnEmulatedFrameWidget: public QWidget, protected DragProcessHandler {
    Q_OBJECT;

    typedef QWidget base_type;

public:
    QnEmulatedFrameWidget(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnEmulatedFrameWidget();

protected:
    /**
     * This function is to be overridden in derived class to provide information
     * on window frame sections.
     * 
     * \param pos                       Position in widget coordinates.
     * \returns                         Window frame section at the given position.
     */
    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPoint &pos) const = 0;

    virtual bool event(QEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;

    virtual void dragMove(DragInfo *info) override;

private:
    friend class QnEmulatedFrameWidgetDragProcessHandler;

    DragProcessor *m_dragProcessor;
    Qt::WindowFrameSection m_section;
    QPoint m_startPinPoint;
    QSize m_startSize;
};


#endif // QN_EMULATED_FRAME_WIDGET_H
