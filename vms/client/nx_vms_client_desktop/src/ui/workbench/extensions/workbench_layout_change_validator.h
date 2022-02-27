// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchLayoutsChangeValidator: public QnWorkbenchContextAware
{
public:
    QnWorkbenchLayoutsChangeValidator(QnWorkbenchContext* context);

    bool confirmChangeVideoWallLayout(const QnLayoutResourcePtr& layout,
        const QnResourceList& removedResources) const;
};
