// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <nx/vms/client/desktop/camera/camera_fwd.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <utils/common/counter_hash.h>

class QnResourceWidget;
class QnMediaResourceWidget;

/**
 * For each Resource Widget on the scene, this class tracks whether it is being currently
 * displayed or not. It provides the necessary getters and signals to track changes of this state.
 */
class QnWorkbenchRenderWatcher:
    public QObject,
    public nx::vms::client::desktop::WindowContextAware
{
    Q_OBJECT

public:
    /**
     * "Fire & forget" constructor. Sets everything up automatically, user just need to connect
     *  to the signals.
     */
    explicit QnWorkbenchRenderWatcher(
        nx::vms::client::desktop::WindowContext* windowContext,
        QObject* parent = nullptr);

    virtual ~QnWorkbenchRenderWatcher() override;

    bool isDisplaying(QnResourceWidget* widget) const;

    bool isDisplaying(const QnResourceDisplayPtr& display) const;

signals:
    /**
     * This signal is emitted whenever a widget starts or stops
     * being displayed. Upon registration widget is considered as not
     * being displayed.
     */
    void widgetChanged(QnResourceWidget* widget);

    void displayChanged(const QnResourceDisplayPtr& display);

private:
    void registerWidget(QnResourceWidget* widget);
    void unregisterWidget(QnResourceWidget* widget);
    void setDisplaying(QnResourceWidget* widget, bool displaying);

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

    QHash<QnResourceWidget*, WidgetData> m_dataByWidget;
    QnCounterHash<QnResourceDisplayPtr> m_countByDisplay;

    // Cannot connect to the main window in the constructor because the workbench is created
    // before the main window.
    bool m_connectedToMainWindow = false;
};
