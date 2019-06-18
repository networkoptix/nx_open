#pragma once

#include <QtCore/QPointer>

#include "abstract_search_synchronizer.h"

namespace nx::vms::client::desktop {

class AnalyticsSearchWidget;
class AnalyticsObjectsVisualizationManager;

/**
 * An utility class to synchronize Right Panel analytics tab state with current media widget
 * analytics selection mode and analytics chunks display on the timeline.
 */
class AnalyticsSearchSynchronizer: public AbstractSearchSynchronizer
{
public:
    AnalyticsSearchSynchronizer(QnWorkbenchContext* context,
        AnalyticsSearchWidget* analyticsSearchWidget, QObject* parent = nullptr);

private:
    bool calculateMediaResourceWidgetAnalyticsEnabled(QnMediaResourceWidget* widget) const;

    void updateAreaSelection();
    void updateTimelineDisplay();
    void updateCachedDevices();
    void updateMediaResourceWidgetAnalyticsMode(QnMediaResourceWidget* widget);
    void updateAllMediaResourceWidgetsAnalyticsMode();

    void handleWidgetAnalyticsFilterRectChanged();

    void setAreaSelectionActive(bool value);

private:
    const QPointer<AnalyticsSearchWidget> m_analyticsSearchWidget;
    const QPointer<AnalyticsObjectsVisualizationManager> m_objectsVisualizationManager;
    QMetaObject::Connection m_activeMediaWidgetConnection;
    bool m_areaSelectionActive = false;
};

} // namespace nx::vms::client::desktop
