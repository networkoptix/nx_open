#ifndef NOTIFYINGWIDGET_P_H
#define NOTIFYINGWIDGET_P_H

#include "pimplbase_p.h"
#include "notifyingwidget.h"

class NotifyingWidgetPrivate: public PimplBasePrivate {
public:
    NotifyingWidgetPrivate():
        doubleClicked(false),
        resizing(false),
        resizingStartedEmitted(false),
        dragging(false),
        draggingStartedEmitted(false)
    {}

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
    bool isDragGrip(Qt::WindowFrameSection section) const;

    void draggingResizingFinished();

private:
    bool doubleClicked;
    bool resizing;
    bool resizingStartedEmitted;
    bool dragging;
    bool draggingStartedEmitted;

private:
    Q_DECLARE_PUBLIC(NotifyingWidget);
};

#endif // NOTIFYINGWIDGET_P_H
