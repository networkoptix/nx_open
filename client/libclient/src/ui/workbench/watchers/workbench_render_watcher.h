#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/counter_hash.h>

class QnResourceWidget;
class QnResourceDisplay;
typedef QSharedPointer<QnResourceDisplay> QnResourceDisplayPtr;
class QnMediaResourceWidget;

/**
 * For each widget on the scene, this class tracks whether it is being
 * currently displayed, or not. It provides the necessary getters and signals
 * to track changes of this state.
 */
class QnWorkbenchRenderWatcher: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
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

    bool isDisplaying(const QnResourceDisplayPtr& display) const;

signals:
    /**
     * This signal is emitted whenever a widget starts or stops
     * being displayed. Upon registration widget is considered as not
     * being displayed.
     *
     * \param widget                    Widget.
     */
    void widgetChanged(QnResourceWidget *widget);

    void displayChanged(const QnResourceDisplayPtr& display);

private:
    void registerWidget(QnResourceWidget *widget);
    void unregisterWidget(QnResourceWidget *widget);
    void setDisplaying(QnResourceWidget *widget, bool displaying);

    void at_beforePaintInstrument_activated();
    void at_afterPaintInstrument_activated();

private:
    struct WidgetData
    {
        WidgetData() {}

        WidgetData(bool displaying, const QnResourceDisplayPtr& display):
            displaying(displaying),
            newDisplaying(false),
            display(display)
        {
        }

        bool displaying = false;
        bool newDisplaying = false;
        QnResourceDisplayPtr display;
    };

    QHash<QnResourceWidget *, WidgetData> m_dataByWidget;
    QnCounterHash<QnResourceDisplayPtr> m_countByDisplay;
};
