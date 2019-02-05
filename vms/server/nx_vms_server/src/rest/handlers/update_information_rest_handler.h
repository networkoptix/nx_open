#pragma once

#include <rest/server/fusion_rest_handler.h>
#include <nx/vms/api/data/software_version.h>

namespace nx::vms::server { class Settings; }
struct UpdateInformationRequestData;

class QnUpdateInformationRestHandler: public QnFusionRestHandler
{
public:
    QnUpdateInformationRestHandler(
        const nx::vms::server::Settings* settings,
        const nx::vms::api::SoftwareVersion& engineVersion);

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
    const nx::vms::server::Settings* m_settings = nullptr;
    const nx::vms::api::SoftwareVersion m_engineVersion;
};
