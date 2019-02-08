#pragma once

#include <common/common_globals.h>

#include <nx/fusion/model_functions_fwd.h>

/** Info about storage rebuild process (if running). */
struct QnStorageScanData
{
    QnStorageScanData();
    QnStorageScanData(Qn::RebuildState state, const QString& path, qreal progress, qreal totalProgress);

    Qn::RebuildState state;     /**< Current process state */
    QString path;               /**< Url of the storage being processed */
    qreal progress;             /**< Progress of the current storage (0.0 to 1.0). */
    qreal totalProgress;        /**< Total progress of the process (0.0 to 1.0). */
};

#define QnStorageScanData_Fields (state)(path)(progress)(totalProgress)

QN_FUSION_DECLARE_FUNCTIONS(QnStorageScanData, (json)(metatype)(eq))

