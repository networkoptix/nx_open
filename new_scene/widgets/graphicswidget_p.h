#ifndef GRAPHICSWIDGET_PRIVATE_H
#define GRAPHICSWIDGET_PRIVATE_H

#include "graphicswidget.h"
#include <cassert>

class GraphicsWidgetPrivate {
public:
    GraphicsWidgetPrivate(GraphicsWidget *qq):
        q_ptr(qq),
        doubleClicked(false),
        resizing(false),
        resizingStartedEmitted(false),
        moving(false),
        movingStartedEmitted(false),
        preDragging(false)
    {
        assert(qq != NULL);
    }

private:
    /**
     * \param section                   Window frame section.
     * \returns                         Whether the given section can be used as a grip for resizing a window.
     */
    bool isResizeGrip(Qt::WindowFrameSection section) const;

    /**
     * \param section                   Window frame section.
     * \returns                         Whether the given section can be used as a grip for dragging a window.
     */
    bool isMoveGrip(Qt::WindowFrameSection section) const;

    void movingResizingFinished();

private:
    bool doubleClicked;
    bool resizing;
    bool resizingStartedEmitted;
    bool moving;
    bool movingStartedEmitted;
    bool preDragging;

    GraphicsWidget::GraphicsExtraFlags extraFlags;

protected:
    GraphicsWidget *q_ptr;

private:
    Q_DECLARE_PUBLIC(GraphicsWidget);
};

#endif // GRAPHICSWIDGET_PRIVATE_H
