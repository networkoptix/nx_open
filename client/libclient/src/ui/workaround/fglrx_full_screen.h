#ifndef QN_FGLRX_FULL_SCREEN_H
#define QN_FGLRX_FULL_SCREEN_H

#include <ui/graphics/instruments/instrument.h>
#include <ui/workbench/workbench_context_aware.h>

/**
 * This class implements a workaround for ATI fglrx drivers that do not perform
 * well in fullscreen OpenGL. For these drivers, this class replaces fullscreen
 * action with a maximize action.
 */
class QnFglrxFullScreen: public Instrument, public QnWorkbenchContextAware {
public:
    QnFglrxFullScreen(QObject *parent = NULL);

protected:
    void updateFglrxMode(bool force = false);

    virtual bool registeredNotify(QWidget *viewport) override;
    virtual void unregisteredNotify(QWidget *viewport) override;

private:
    bool m_fglrxMode;
};


#endif // QN_FGLRX_FULL_SCREEN_H
