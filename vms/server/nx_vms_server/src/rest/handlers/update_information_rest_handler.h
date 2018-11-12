#pragma once

#include <rest/server/fusion_rest_handler.h>

namespace nx::mediaserver { class Settings; }
struct UpdateInformationRequestData;

class QnUpdateInformationRestHandler: public QnFusionRestHandler
{
public:
    QnUpdateInformationRestHandler(const nx::mediaserver::Settings* settings);

    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor*processor) override;
private:
    qint64 freeSpaceForUpdate() const;
    int checkInternetForUpdate(
        const QString& publicationKey,
        QByteArray* result,
        QByteArray* contentType,
        const UpdateInformationRequestData& request) const;

private:
    const nx::mediaserver::Settings* m_settings = nullptr;
};
