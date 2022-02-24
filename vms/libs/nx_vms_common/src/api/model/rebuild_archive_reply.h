// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>

#include <nx/fusion/model_functions_fwd.h>

/**%apidoc Info about Storage rebuild process (if running). */
struct NX_VMS_COMMON_API QnStorageScanData
{
    /**%apidoc Current process state. */
    Qn::RebuildState state = Qn::RebuildState::RebuildState_None;

    /**%apidoc Url of the Storage being processed. */
    QString path;

    /**%apidoc Progress of the current Storage (0.0 to 1.0). */
    qreal progress = 0.0;

    /**%apidoc Total progress of the process (0.0 to 1.0). */
    qreal totalProgress = 0.0;

    QString toString() const;

    bool operator==(const QnStorageScanData& other) const = default;
};

#define QnStorageScanData_Fields (state)(path)(progress)(totalProgress)

QN_FUSION_DECLARE_FUNCTIONS(QnStorageScanData, (json)(metatype), NX_VMS_COMMON_API)
