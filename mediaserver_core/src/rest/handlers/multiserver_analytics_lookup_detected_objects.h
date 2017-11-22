#pragma once

#include <QtCore/QString>

#include <rest/server/fusion_rest_handler.h>

namespace nx {
namespace analytics {
namespace storage {

class AbstractEventsStorage;
struct Filter;

} // namespace storage
} // namespace analytics
} // namespace nx

class QnMultiserverAnalyticsLookupDetectedObjects:
    public QnRestRequestHandler
{
public:
    QnMultiserverAnalyticsLookupDetectedObjects(
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
    nx::analytics::storage::AbstractEventsStorage* m_eventStorage;

    bool deserializeRequest(
        const QnRequestParamList& params,
        nx::analytics::storage::Filter* filter,
        Qn::SerializationFormat* outputFormat);

    template<typename T>
    QByteArray serializeOutputData(
        const T& output,
        Qn::SerializationFormat outputFormat);
};
