
#include "preview_search_duration_metric.h"

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>

PreviewSearchDurationMetric::PreviewSearchDurationMetric(QnWorkbench *workbench)
    : base_type()
    , TimeDurationMetric()
{
    if (!workbench)
        return;

    QPointer<QnWorkbench> workbenchGuard(workbench);
    QPointer<PreviewSearchDurationMetric> guard(this);

    const auto layoutChangedHandler =
        [this, guard, workbenchGuard]()
    {
        if (!guard || !workbenchGuard)
            return;

        const auto layout = workbenchGuard->currentLayout();
        const bool isPreviewSearchLayout = (layout
            && layout->isSearchLayout());

        activateCounter(isPreviewSearchLayout);
    };

    connect(workbench, &QnWorkbench::currentLayoutChanged
        , this, layoutChangedHandler);
}

PreviewSearchDurationMetric::~PreviewSearchDurationMetric()
{}
