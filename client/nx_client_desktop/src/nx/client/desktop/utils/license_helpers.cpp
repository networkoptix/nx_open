#include "license_helpers.h"

#include <QtCore/QThread>

#include <nx/network/http/asynchttpclient.h>
#include <nx/fusion/serialization_format.h>
#include <nx/utils/raii_guard.h>

#include <rest/server/json_rest_result.h>
#include <utils/common/delayed.h>
#include <utils/common/connective.h>

namespace {

static const auto kDeactivateLicenseUrl =
    QUrl::fromUserInput(lit("http://nxlicensed.hdw.mx/nxlicensed/api/v1/deactivate"));
static const auto kJsonTemplate = lit(
    "{ \"licenses\": [ {\"key\": \"key1\", \"hwid\": \"%1\"} ] }");
static const QByteArray kJsonContentType(
    Qn::serializationFormatToHttpContentType(Qn::JsonFormat));


class LicenseDeactivatorPrivate: public Connective<QObject>
{
    using base_type = Connective<QObject>;
    using DeactivationHandler = nx::client::desktop::helpers::license::DeactivationHandler;
    using DeactivationResult = nx::client::desktop::helpers::license::DeactivationResult;

public:
    LicenseDeactivatorPrivate(
        const QString& hwid,
        const DeactivationHandler& handler);

private:
    void finalize(DeactivationResult result);

private:
    const nx_http::AsyncHttpClientPtr m_httpClient;
    const DeactivationHandler m_handler;
};

LicenseDeactivatorPrivate::LicenseDeactivatorPrivate(
    const QString& hwid,
    const DeactivationHandler& handler)
    :
    base_type(),
    m_httpClient(nx_http::AsyncHttpClient::create()),
    m_handler(handler)
{
    if (hwid.isEmpty())
    {
        NX_EXPECT(false, "Hardware id can't be empty");
        finalize(DeactivationResult::InvalidParameters);
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

    const auto body = kJsonTemplate.arg(hwid).toLatin1();
    m_httpClient->doPost(kDeactivateLicenseUrl, kJsonContentType, body);
}

void LicenseDeactivatorPrivate::finalize(DeactivationResult result)
{
    const auto deleteLaterGuard = QnRaiiGuard::createDestructible(
        [this]() { deleteLater(); });

    if (thread() != QThread::currentThread())
        NX_EXPECT(false, "Deactivation handler is tring to be called in wrong thread");
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
    const QString& hwid,
    const DeactivationHandler& completionHandler)
{
    //
    new LicenseDeactivatorPrivate(hwid, completionHandler);
}

} // license namespace
} // helpers namespace
} // desktop namespace
} // client namespace
} // nx namespace
