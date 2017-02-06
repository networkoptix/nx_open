#include "workbench_render_watcher.h"

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

#include <camera/abstract_renderer.h>

#include <ui/workbench/workbench_display.h>
#include <ui/graphics/items/resource/resource_widget_renderer.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/forwarding_instrument.h>


QnWorkbenchRenderWatcher::QnWorkbenchRenderWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    /* Connect to display. */
    connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
        &QnWorkbenchRenderWatcher::registerWidget);
    connect(display(), &QnWorkbenchDisplay::widgetAboutToBeRemoved, this,
        &QnWorkbenchRenderWatcher::unregisterWidget);

    auto signalActivated = static_cast<void (SignalingInstrument::*)(QWidget *, QEvent *)>
        (&SignalingInstrument::activated);

    connect(display()->beforePaintInstrument(), signalActivated, this,
        &QnWorkbenchRenderWatcher::at_beforePaintInstrument_activated);

    connect(display()->afterPaintInstrument(), signalActivated, this,
        &QnWorkbenchRenderWatcher::at_afterPaintInstrument_activated);
}

QnWorkbenchRenderWatcher::~QnWorkbenchRenderWatcher()
{
    NX_ASSERT(m_countByDisplay.keys().isEmpty());
}

bool QnWorkbenchRenderWatcher::isDisplaying(QnResourceWidget *widget) const
{
    return m_dataByWidget.value(widget).displaying;
}

bool QnWorkbenchRenderWatcher::isDisplaying(const QnResourceDisplayPtr& display) const
{
    return m_countByDisplay.contains(display);
}

void QnWorkbenchRenderWatcher::setDisplaying(QnResourceWidget *widget, bool displaying)
{
    WidgetData &data = m_dataByWidget[widget];
    if (data.displaying == displaying)
        return;

    data.displaying = displaying;

    emit widgetChanged(widget);

    if (data.display)
    {
        /* Not all widgets have an associated display. */
        if (displaying)
        {
            if (m_countByDisplay.insert(data.display))
                emit displayChanged(data.display);
        }
        else
        {
            if (m_countByDisplay.remove(data.display))
                emit displayChanged(data.display);
        }
    }
}

void QnWorkbenchRenderWatcher::registerWidget(QnResourceWidget *widget)
{
    NX_ASSERT(widget);
    if (!widget)
        return;

    NX_ASSERT(!m_dataByWidget.contains(widget));
    if (m_dataByWidget.contains(widget))
        return;

    connect(widget, &QnResourceWidget::painted, this, [this, widget]
        {
            m_dataByWidget[widget].newDisplaying = true;
        });

    QnResourceDisplayPtr display;
    if (QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
    {
        display = mediaWidget->display();

        connect(mediaWidget, &QnMediaResourceWidget::displayChanged, this,
            [this, mediaWidget]
            {
                bool isDisplaying = this->isDisplaying(mediaWidget);

                setDisplaying(mediaWidget, false);
                m_dataByWidget[mediaWidget].display = mediaWidget->display();
                setDisplaying(mediaWidget, isDisplaying);
            });
    }

    m_dataByWidget[widget] = WidgetData(false, display);
}

void QnWorkbenchRenderWatcher::unregisterWidget(QnResourceWidget *widget)
{
    NX_ASSERT(widget);
    if (!widget)
        return;

    if (!m_dataByWidget.contains(widget))
        return; /* It's OK to unregister a widget that is not there. */

    widget->disconnect(this);

    auto data = m_dataByWidget.take(widget);
    if (data.display && data.displaying)
        m_countByDisplay.remove(data.display);
}



// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchRenderWatcher::at_beforePaintInstrument_activated()
{
    for (auto pos = m_dataByWidget.begin(); pos != m_dataByWidget.end(); pos++)
        pos->newDisplaying = false;
}

void QnWorkbenchRenderWatcher::at_afterPaintInstrument_activated()
{
    for (auto pos = m_dataByWidget.begin(); pos != m_dataByWidget.end(); pos++)
        setDisplaying(pos.key(), pos->newDisplaying);
}
