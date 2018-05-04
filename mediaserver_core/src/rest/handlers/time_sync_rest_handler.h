#pragma once

#include "rest/server/request_handler.h"
#include <nx/time_sync/time_sync_manager.h>

namespace nx {
namespace mediaserver {

class TimeSyncManager;

class QnTimeSyncRestHandler: public QnRestRequestHandler
{
public:
    /** Contains peer's time synchronization information. */

    QnTimeSyncRestHandler();

    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor*) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* owner) override;
};

} // namespace mediaserver
} // namespace nx
