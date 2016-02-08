
#pragma once

#include <rest/server/fusion_rest_handler.h>

class StatisticsActionHandler;
typedef QSharedPointer<StatisticsActionHandler> StatisticsActionHandlerPtr;

class QnMultiserverStatisticsRestHandler : public QnFusionRestHandler
{
    typedef QnFusionRestHandler base_type;

public:
    QnMultiserverStatisticsRestHandler(const QString &basePath);

    ~QnMultiserverStatisticsRestHandler();

    int executeGet(const QString& path
        , const QnRequestParamList& params
        , QByteArray& result
        , QByteArray& contentType
        , const QnRestConnectionProcessor *processor) override;

private:
    typedef QHash<QString, StatisticsActionHandlerPtr> HandlersHash;

    HandlersHash m_handlers;
};