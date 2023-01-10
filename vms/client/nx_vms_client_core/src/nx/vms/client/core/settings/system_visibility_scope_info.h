// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions_fwd.h>

struct SystemVisibilityScopeInfo
{
    QString name;
    int visibilityScope;
};
#define SystemVisibilityScopeInfo_Fields (name)(visibilityScope)

QN_FUSION_DECLARE_FUNCTIONS(SystemVisibilityScopeInfo, (json))

typedef QHash<QnUuid, SystemVisibilityScopeInfo> SystemVisibilityScopeInfoHash;
