#pragma once

#include <ui/graphics/instruments/instrument.h>
#include <ui/workbench/workbench_context_aware.h>

class QOpenGLWidget;

/**
 * This class implements a workaround for ATI fglrx drivers that do not perform
 * well in fullscreen OpenGL. For these drivers, this class replaces fullscreen
 * action with a maximize action.
 */
class QnFglrxFullScreen: public Instrument, public QnWorkbenchContextAware
{
    using base_type = Instrument;

public:
    QnFglrxFullScreen(QObject *parent = NULL);

protected:
    void updateFglrxMode(QOpenGLWidget* viewport, bool force = false);

    virtual bool registeredNotify(QWidget *viewport) override;
    virtual void unregisteredNotify(QWidget *viewport) override;

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool m_fglrxMode;
};
