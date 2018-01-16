#pragma once

#include <QtCore/QString>

#include <analytics/detected_objects_storage/analytics_events_storage.h>
#include <rest/server/fusion_rest_handler.h>

class QnCommonModule;

class QnMultiserverAnalyticsLookupDetectedObjects:
    public QnRestRequestHandler
{
public:
    QnMultiserverAnalyticsLookupDetectedObjects(
        QnCommonModule* commonModule,
        const QString& path,
        nx::analytics::storage::AbstractEventsStorage* eventStorage);

    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* processor) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* owner) override;

private:
    const QString m_requestPath;
    nx::analytics::storage::AbstractEventsStorage* m_eventStorage = nullptr;
    QnCommonModule* m_commonModule = nullptr;

    bool deserializeRequest(
        const QnRequestParamList& params,
        nx::analytics::storage::Filter* filter,
        Qn::SerializationFormat* outputFormat);

    bool deserializeOutputFormat(
        const QnRequestParamList& params,
        Qn::SerializationFormat* outputFormat);

    bool deserializeRequest(
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        nx::analytics::storage::Filter* filter,
        Qn::SerializationFormat* outputFormat);

    nx::network::http::StatusCode::Value execute(
        const nx::analytics::storage::Filter& filter,
        bool isLocal,
        Qn::SerializationFormat outputFormat,
        QByteArray* body,
        QByteArray* contentType);

    nx::network::http::StatusCode::Value lookupOnEveryOtherServer(
        const nx::analytics::storage::Filter& filter,
        std::vector<nx::analytics::storage::LookupResult>* lookupResults);

    nx::analytics::storage::LookupResult aggregateResults(
        std::vector<nx::analytics::storage::LookupResult> lookupResults,
        Qt::SortOrder resultSortOrder);

    template<typename T>
    QByteArray serializeOutputData(
        const T& output,
        Qn::SerializationFormat outputFormat);
};
