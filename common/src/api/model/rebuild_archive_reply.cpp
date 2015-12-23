#include "rebuild_archive_reply.h"

QnStorageScanData::QnStorageScanData()
    : state(Qn::RebuildState_None)
    , path()
    , progress(0.0)
    , totalProgress(0.0)
{}

QnStorageScanData::QnStorageScanData( Qn::RebuildState state, const QString& path, qreal progress, qreal totalProgress )
    : state(state)
    , path(path)
    , progress(progress)
    , totalProgress(totalProgress)
{

}
