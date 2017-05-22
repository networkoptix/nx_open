#include "license_helpers.h"

#include <QtCore/QThread>

#include <nx/network/http/asynchttpclient.h>
#include <nx/fusion/serialization_format.h>
#include <nx/utils/raii_guard.h>

#include <rest/server/json_rest_result.h>
#include <utils/common/delayed.h>
#include <utils/common/connective.h>

#include <licensing/license.h>

namespace {

static const auto kDeactivateLicenseUrl =
    QUrl::fromUserInput(lit("http://nxlicensed.hdw.mx/nxlicensed/api/v1/deactivate"));

static const auto kJsonRootTemplate = lit(
    "{ \"licenses\": [%1] }");

static const QByteArray kJsonContentType(
    Qn::serializationFormatToHttpContentType(Qn::JsonFormat));

QString toJson(const QnLicensePtr& license)
{
    return lit("{ \"key\": \"%1\", \"hwid\": \"%2\" }").arg(
        QString::fromStdString(license->key().toStdString()), license->hardwareId());
}

class LicenseDeactivatorPrivate: public Connective<QObject>
{
    using base_type = Connective<QObject>;
    using DeactivationHandler = nx::client::desktop::helpers::license::DeactivationHandler;
    using DeactivationResult = nx::client::desktop::helpers::license::DeactivationResult;

public:
    LicenseDeactivatorPrivate(
        const QnLicenseList& licenses,
        const DeactivationHandler& handler);

private:
    void finalize(DeactivationResult result);

private:
    const nx_http::AsyncHttpClientPtr m_httpClient;
    const DeactivationHandler m_handler;
};

LicenseDeactivatorPrivate::LicenseDeactivatorPrivate(
    const QnLicenseList& licenses,
    const DeactivationHandler& handler)
    :
    base_type(),
    m_httpClient(nx_http::AsyncHttpClient::create()),
    m_handler(handler)
{
    if (licenses.isEmpty())
    {
        finalize(DeactivationResult::Success);
        return;
    }

    const auto guard = QPointer<LicenseDeactivatorPrivate>(this);
    const auto postHandler =
        [this, guard]()
        {
            if (m_httpClient->failed())
            {
                finalize(DeactivationResult::ServerError);
                return;
            }

            const auto response = m_httpClient->response();
            if (!response)
            {
                finalize(DeactivationResult::UnspecifiedError);
                return;
            }

            switch(response->statusLine.statusCode)
            {
                case nx_http::StatusCode::ok:
                    finalize(DeactivationResult::Success);
                    break;
                case nx_http::StatusCode::badRequest:
                    finalize(DeactivationResult::DeactivationError);
                    break;
                case nx_http::StatusCode::internalServerError:
                    finalize(DeactivationResult::ServerError);
                    break;
                default:
                    finalize(DeactivationResult::UnspecifiedError);
                    break;
            }
        };

    const auto threadSafeHandler =
        [this, guard, postHandler](nx_http::AsyncHttpClientPtr /*client*/)
        {
            if (guard)
                executeDelayed(postHandler, 0, thread());
        };

    connect(m_httpClient.get(), &nx_http::AsyncHttpClient::done, this, threadSafeHandler,
        Qt::DirectConnection);

    QStringList licenseData;
    for (const auto& license: licenses)
        licenseData.append(toJson(license));

    const auto body = kJsonRootTemplate.arg(licenseData.join(L',')).toLatin1();
    qDebug() << body;
    m_httpClient->doPost(kDeactivateLicenseUrl, kJsonContentType, body);
}

void LicenseDeactivatorPrivate::finalize(DeactivationResult result)
{
    const auto deleteLaterGuard = QnRaiiGuard::createDestructible(
        [this]() { deleteLater(); });

    if (thread() != QThread::currentThread())
        NX_EXPECT(false, "Deactivation handler is trying to be called in wrong thread");
    else if (m_handler)
        m_handler(result);
}

}

//------------------------------------------------------------------------------------------------

namespace nx {
namespace client {
namespace desktop {
namespace helpers {
namespace license {

void deactivateLicenseAsync(
    const QnLicenseList& licenses,
    const DeactivationHandler& completionHandler)
{
    // Deactivator will delete himself when after operation complete
    new LicenseDeactivatorPrivate(licenses, completionHandler);
}

} // license namespace
} // helpers namespace
} // desktop namespace
} // client namespace
} // nx namespace
