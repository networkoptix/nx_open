
#include "preview_search_duration_metric.h"

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>

PreviewSearchDurationMetric::PreviewSearchDurationMetric(QnWorkbench *workbench)
    : base_type()
    , TimeDurationMetric()
    , m_workbench(workbench)
{
    if (!m_workbench)
        return;

    QPointer<PreviewSearchDurationMetric> guard(this);
    const auto layoutChangedHandler = [this, guard]()
    {
        if (!guard || !m_workbench)
            return;

        const auto layout = m_workbench->currentLayout();
        const bool isPreviewSearchLayout = (layout
            && layout->isSearchLayout());

        activateCounter(isPreviewSearchLayout);
    };

    connect(m_workbench, &QnWorkbench::currentLayoutChanged
        , this, layoutChangedHandler);
}

PreviewSearchDurationMetric::~PreviewSearchDurationMetric()
{}
