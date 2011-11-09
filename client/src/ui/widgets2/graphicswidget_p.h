#ifndef GRAPHICSWIDGET_P_H
#define GRAPHICSWIDGET_P_H

#include "graphicswidget.h"

class GraphicsWidgetPrivate
{
    Q_DECLARE_PUBLIC(GraphicsWidget)

public:
    GraphicsWidgetPrivate() :
        q_ptr(0),
        doubleClicked(false),
        resizing(false),
        resizingStartedEmitted(false),
        moving(false),
        movingStartedEmitted(false),
        preDragging(false),
        isUnderMouse(false)
    {}
    virtual ~GraphicsWidgetPrivate()
    {}

protected:
    /**
     * \param section                   Window frame section.
     * \returns                         Whether the given section can be used as a grip for resizing a window.
     */
    bool isResizeGrip(Qt::WindowFrameSection section) const
    { return section != Qt::NoSection && section != Qt::TitleBarArea; }

    /**
     * \param section                   Window frame section.
     * \returns                         Whether the given section can be used as a grip for moving a window.
     */
    inline bool isMoveGrip(Qt::WindowFrameSection section) const
    { return section == Qt::TitleBarArea; }

    void movingResizingFinished();

protected:
    GraphicsWidget *q_ptr;

private:
    // packed 32 bit
    uint doubleClicked : 1;
    uint resizing : 1;
    uint resizingStartedEmitted : 1;
    uint moving : 1;
    uint movingStartedEmitted : 1;
    uint preDragging : 1;
    // QTBUG-18797: When setting the flag ItemIgnoresTransformations for an item,
    // it will receive mouse events as if it was transformed by the view.
    uint isUnderMouse : 1;
    uint reserved : 26;

    GraphicsWidget::GraphicsExtraFlags extraFlags;
};

#endif // GRAPHICSWIDGET_P_H
