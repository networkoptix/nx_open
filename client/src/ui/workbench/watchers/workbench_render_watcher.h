#ifndef QN_WORKBENCH_RENDER_WATCHER_H
#define QN_WORKBENCH_RENDER_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <ui/workbench/workbench_context_aware.h>

class QnResourceWidget;
class QnAbstractRenderer;

/**
 * For each renderer on the scene, this class tracks whether it is being 
 * currently displayed, or not. It provides the necessary getters and signals
 * to track changes of this state.
 */
class QnWorkbenchRenderWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    /**
     * "Fire & forget" constructor. Sets everything up automatically, user
     * just needs to connect to the signals.
     * 
     * \param parent                    Context-aware parent.
     */
    QnWorkbenchRenderWatcher(QObject *parent = NULL);

    /**
     * \param renderer                  Renderer to get displaying state for.
     * \returns                         Whether the given renderer is currently
     *                                  being displayed.
     */
    bool isDisplaying(QnAbstractRenderer *renderer) const;

signals:
    /**
     * This signal is emitted whenever a renderer starts or stops
     * being displayed. Upon registration renderer is considered as not 
     * being displayed.
     * 
     * \param renderer                  Renderer.
     * \param displaying                New displaying state of the renderer.
     */
    void displayingChanged(QnAbstractRenderer *renderer, bool displaying);

protected:
    /**
     * Registers a renderer. Note that the actual class of the passed object
     * must be <tt>QObject</tt>-derived so that its lifetime can be tracked.
     * 
     * \param renderer                  Renderer to register.
     */
    void registerRenderer(QnAbstractRenderer *renderer);

    /**
     * Unregisters the given renderer.
     * 
     * \param renderer                  Renderer to unregister.
     */
    void unregisterRenderer(QnAbstractRenderer *renderer);

private slots:
    void at_beforePaintInstrument_activated();
    void at_afterPaintInstrument_activated();
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);
    void at_lifetime_destroyed();

private:
    struct Info {
        Info(): displayCounter(0), displaying(false), lifetime(NULL) {}
        Info(int displayCounter, bool displaying, QObject *lifetime): displayCounter(displayCounter), displaying(displaying), lifetime(lifetime) {}

        int displayCounter;
        bool displaying;
        QObject *lifetime;
    };

    QHash<QnAbstractRenderer *, Info> m_infoByRenderer;
    QHash<QObject *, QnAbstractRenderer *> m_rendererByLifetime;
};

#endif // QN_WORKBENCH_RENDER_WATCHER_H
