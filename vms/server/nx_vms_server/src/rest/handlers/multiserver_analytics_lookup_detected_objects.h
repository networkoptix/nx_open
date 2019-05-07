#pragma once

#include <QtCore/QString>

#include <analytics/db/abstract_storage.h>
#include <rest/server/fusion_rest_handler.h>

class QnCommonModule;

// TODO: Rename with suffix "RestHandler".
class QnMultiserverAnalyticsLookupDetectedObjects:
    public QnRestRequestHandler
{
public:
    QnMultiserverAnalyticsLookupDetectedObjects(
        QnCommonModule* commonModule,
        nx::analytics::db::AbstractEventsStorage* eventStorage);

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
    QnCommonModule* m_commonModule = nullptr;
    nx::analytics::db::AbstractEventsStorage* m_eventStorage = nullptr;

    bool deserializeRequest(
        const QnRequestParamList& params,
        nx::analytics::db::Filter* filter,
        Qn::SerializationFormat* outputFormat);

    bool deserializeOutputFormat(
        const QnRequestParamList& params,
        Qn::SerializationFormat* outputFormat);

    bool deserializeRequest(
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        nx::analytics::db::Filter* filter,
        Qn::SerializationFormat* outputFormat);

    nx::network::http::StatusCode::Value execute(
        const nx::analytics::db::Filter& filter,
        bool isLocal,
        Qn::SerializationFormat outputFormat,
        QByteArray* body,
        QByteArray* contentType);

    nx::network::http::StatusCode::Value lookupOnEveryOtherServer(
        const nx::analytics::db::Filter& filter,
        std::vector<nx::analytics::db::LookupResult>* lookupResults);

    nx::analytics::db::LookupResult mergeResults(
        std::vector<nx::analytics::db::LookupResult> lookupResults,
        Qt::SortOrder resultSortOrder);

    template<typename T>
    QByteArray serializeOutputData(
        const T& output,
        Qn::SerializationFormat outputFormat);
};
