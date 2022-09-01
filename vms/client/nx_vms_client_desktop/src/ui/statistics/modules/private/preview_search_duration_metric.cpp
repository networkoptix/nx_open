// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "preview_search_duration_metric.h"

#include <QtCore/QPointer>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>

PreviewSearchDurationMetric::PreviewSearchDurationMetric(QnWorkbench *workbench)
    : base_type()
    , QnTimeDurationMetric()
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
            && layout->isPreviewSearchLayout());

        setCounterActive(isPreviewSearchLayout);
    };

    connect(workbench, &QnWorkbench::currentLayoutChanged
        , this, layoutChangedHandler);
}

PreviewSearchDurationMetric::~PreviewSearchDurationMetric()
{}
