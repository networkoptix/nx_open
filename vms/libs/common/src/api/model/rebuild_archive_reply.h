#pragma once

#include <common/common_globals.h>

#include <nx/fusion/model_functions_fwd.h>

/** Info about storage rebuild process (if running). */
struct QnStorageScanData
{
    /** Current process state */
    Qn::RebuildState state = Qn::RebuildState::RebuildState_None;

    /** Url of the storage being processed. */
    QString path;

    /** Progress of the current storage (0.0 to 1.0). */
    qreal progress = 0.0;

    /** Total progress of the process (0.0 to 1.0). */
    qreal totalProgress = 0.0;

    QString toString() const;
};

#define QnStorageScanData_Fields (state)(path)(progress)(totalProgress)

QN_FUSION_DECLARE_FUNCTIONS(QnStorageScanData, (json)(metatype)(eq))
