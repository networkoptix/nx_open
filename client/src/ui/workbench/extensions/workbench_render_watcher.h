#ifndef QN_WORKBENCH_RENDER_WATCHER_H
#define QN_WORKBENCH_RENDER_WATCHER_H

#include <QObject>
#include <QHash>

class QnWorkbenchDisplay;
class QnResourceWidget;

class QnAbstractRenderer;

class QnWorkbenchRenderWatcher: public QObject {
    Q_OBJECT;
public:
    /**
     * "Fire & forget" constructor. Sets everything up automatically, user
     * just needs to connect to the signals.
     * 
     * \param display                   Workbench display to work with.
     * \param parent                    Parent object.
     */
    QnWorkbenchRenderWatcher(QnWorkbenchDisplay *display, QObject *parent = NULL);

    /**
     * Manual constructor. User will have to call registration and notification 
     * functions manually.
     * 
     * \param parent                    Parent object.
     */
    QnWorkbenchRenderWatcher(QObject *parent = NULL);

    /**
     * Registers a renderer.
     * 
     * \param renderer                  Renderer to register.
     * \param lifetime                  QObject that has the same lifetime as
     *                                  the given renderer. It will be used
     *                                  for automatic unregistration on
     *                                  renderer destruction.
     */
    void registerRenderer(QnAbstractRenderer *renderer, QObject *lifetime);

    /**
     * Unregisters the given renderer.
     * 
     * \param renderer                  Renderer to unregister.
     */
    void unregisterRenderer(QnAbstractRenderer *renderer);

public slots:
    /**
     * Notifies this watcher that scene rendering has started.
     */
    void startDisplay();

    /**
     * Notifies this watcher that scene rendering has ended.
     */
    void finishDisplay();

signals:
    /**
     * This signal is emitted whenever a renderer starts or stops
     * being displayed. Upon registration renderer is considered as not 
     * being displayed.
     * 
     * \param renderer                  Renderer.
     * \param displaying                New displaying state of the renderer.
     */
    void displayingStateChanged(QnAbstractRenderer *renderer, bool displaying);

protected slots:
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
