#pragma once

#include "rest/server/request_handler.h"

namespace nx {
namespace mediaserver {

class TimeSyncManager;

class QnTimeSyncRestHandler: public QnRestRequestHandler
{
public:
    /** Contains peer's time synchronization information. */

    QnTimeSyncRestHandler(TimeSyncManager* timeSyncManager);

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

private:
    TimeSyncManager* m_timeSyncManager = nullptr;
};

} // namespace mediaserver
} // namespace nx
