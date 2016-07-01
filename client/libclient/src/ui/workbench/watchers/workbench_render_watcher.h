#ifndef QN_WORKBENCH_RENDER_WATCHER_H
#define QN_WORKBENCH_RENDER_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <ui/workbench/workbench_context_aware.h>

class QnResourceWidget;
class QnResourceDisplay;
class QnMediaResourceWidget;

/**
 * For each widget on the scene, this class tracks whether it is being 
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
     * Virtual destructor.
     */
    virtual ~QnWorkbenchRenderWatcher();

    /**
     * \param widget                    Widget to get displaying state for.
     * \returns                         Whether the given widget is currently
     *                                  being displayed.
     */
    bool isDisplaying(QnResourceWidget *widget) const;

    bool isDisplaying(QnResourceDisplay *display) const;

signals:
    /**
     * This signal is emitted whenever a widget starts or stops
     * being displayed. Upon registration widget is considered as not 
     * being displayed.
     * 
     * \param widget                    Widget.
     */
    void widgetChanged(QnResourceWidget *widget);

    void displayChanged(QnResourceDisplay *display);

private:
    Q_SLOT void registerWidget(QnResourceWidget *widget);
    Q_SLOT void unregisterWidget(QnResourceWidget *widget);
    void setDisplaying(QnResourceWidget *widget, bool displaying);

private slots:
    void at_beforePaintInstrument_activated();
    void at_afterPaintInstrument_activated();
    void at_widget_displayChanged();
    void at_widget_displayChanged(QnMediaResourceWidget *widget);
    void at_widget_painted();
    void at_widget_painted(QnResourceWidget *widget);

private:
    struct WidgetData {
        WidgetData(): displaying(false), newDisplaying(false), display(NULL) {}
        WidgetData(bool displaying, QnResourceDisplay *display): displaying(displaying), newDisplaying(false), display(display) {}

        bool displaying;
        bool newDisplaying;
        QnResourceDisplay *display;
    };

    QHash<QnResourceWidget *, WidgetData> m_dataByWidget;
    QHash<QnResourceDisplay *, int> m_countByDisplay;
};

#endif // QN_WORKBENCH_RENDER_WATCHER_H
