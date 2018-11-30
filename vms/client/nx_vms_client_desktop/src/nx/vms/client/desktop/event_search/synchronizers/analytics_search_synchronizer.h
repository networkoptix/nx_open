#pragma once

#include <QtCore/QPointer>

#include "abstract_search_synchronizer.h"

namespace nx::vms::client::desktop {

class AnalyticsSearchWidget;

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
    void updateAreaSelection();
    void updateTimelineDisplay();

private:
    const QPointer<AnalyticsSearchWidget> m_analyticsSearchWidget;
};

} // namespace nx::vms::client::desktop
