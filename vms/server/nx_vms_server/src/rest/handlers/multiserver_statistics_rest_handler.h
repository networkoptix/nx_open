
#pragma once

#include <rest/server/fusion_rest_handler.h>

class AbstractStatisticsActionHandler;
typedef QSharedPointer<AbstractStatisticsActionHandler> StatisticsActionHandlerPtr;

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

    int executePost(const QString& path
        , const QnRequestParamList& params
        , const QByteArray& body
        , const QByteArray& srcBodyContentType
        , QByteArray& result
        , QByteArray& resultContentType
        , const QnRestConnectionProcessor *processor)  override;

private:
    typedef std::function<int (const StatisticsActionHandlerPtr &handler)> RunHandlerFunc;
    int processRequest(const QString &fullPath, const RunHandlerFunc &runHandler);

private:
    typedef QHash<QString, StatisticsActionHandlerPtr> HandlersHash;
    HandlersHash m_handlers;
};