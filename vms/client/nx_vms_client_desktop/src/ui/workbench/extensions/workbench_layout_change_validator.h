// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchLayoutsChangeValidator: public QnWorkbenchContextAware
{
public:
    QnWorkbenchLayoutsChangeValidator(QnWorkbenchContext* context);

    bool confirmChangeVideoWallLayout(const QnLayoutResourcePtr& layout,
        const QnResourceList& removedResources) const;

    bool confirmLoadVideoWallMatrix(const QnVideoWallMatrixIndex& matrixIndex) const;
    bool confirmDeleteVideoWallMatrices(const QnVideoWallMatrixIndexList& matrixIndexes) const;

private:
    void addLayoutResourceIds(const QnLayoutResourcePtr& layout, QSet<QnUuid>& resourceIds) const;

    bool isAccessibleBySpecifiedProvidersOnly(
        const QnResourcePtr& resource, const QSet<QnResourcePtr>& specificProviders) const;
};
