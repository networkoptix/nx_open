#ifndef REBUILD_ARCHIVE_REPLY_H
#define REBUILD_ARCHIVE_REPLY_H

#include <QtCore/QMetaType>

#include <utils/common/model_functions_fwd.h>

struct QnStorageScanData
{
    QnStorageScanData(): state(Qn::RebuildState_Unknown), progress(0.0) {}
    QnStorageScanData(Qn::RebuildState state, const QString& path, qreal progress): state(state), path(path), progress(progress) {}

    Qn::RebuildState state;
    QString path;
    qreal progress;
};
#define QnStorageScanData_Fields (state)(path)(progress)

QN_FUSION_DECLARE_FUNCTIONS(QnStorageScanData, (json)(metatype))


#include <QtCore/QMetaType>


#endif // REBUILD_ARCHIVE_REPLY_H
