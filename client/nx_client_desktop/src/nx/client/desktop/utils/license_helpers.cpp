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
        QString::fromLatin1(license->key().constData()), license->hardwareId());
}

namespace license = nx::client::desktop::helpers::license;
using Error = license::Deactivator::ErrorCode;
using LicenseErrorHash = license::Deactivator::LicenseErrorHash;

license::Deactivator::ErrorCode getError(const QJsonObject& object)
{
    static const auto kCodeTag = lit("code");
    if (object.isEmpty() || !object.contains(kCodeTag))
        return Error::UnknownError;

    const auto code = object[kCodeTag].toString();
    if (code == lit("keyIsNotActivated"))
        return Error::LicenseDeactivatedAlready;

    if (code == lit("limitExceeded"))
        return Error::LimitExceeded;

    return Error::UnknownError;
}

LicenseErrorHash extractErrors(const QByteArray& messageBody)
{
    const auto document = QJsonDocument::fromJson(messageBody);
    if (document.isEmpty())
        return LicenseErrorHash();

    const auto object = document.object();
    if (object.isEmpty())
        return LicenseErrorHash();

    const auto licenseErrors = object[lit("errors")].toObject()[lit("licenseErrors")].toObject();
    if (licenseErrors.isEmpty())
        return LicenseErrorHash();

    LicenseErrorHash result;
    for (const auto& key: licenseErrors.keys())
        result.insert(key.toLatin1(), getError(licenseErrors[key].toObject()));

    return result;
}

class LicenseDeactivatorPrivate: public Connective<QObject>
{
    using base_type = Connective<QObject>;
    using Handler = license::Deactivator::Handler;
    using Result = license::Deactivator::Result;

public:
    LicenseDeactivatorPrivate(
        const QnLicenseList& licenses,
        const Handler& handler,
        QObject* parent);

private:
    void finalize(
        Result result,
        const LicenseErrorHash& errors = LicenseErrorHash());

private:
    const nx_http::AsyncHttpClientPtr m_httpClient;
    const Handler m_handler;
};

LicenseDeactivatorPrivate::LicenseDeactivatorPrivate(
    const QnLicenseList& licenses,
    const Handler& handler,
    QObject* parent)
    :
    base_type(parent),
    m_httpClient(nx_http::AsyncHttpClient::create()),
    m_handler(handler)
{
    if (licenses.isEmpty())
    {
        finalize(Result::Success);
        return;
    }

    const auto guard = QPointer<LicenseDeactivatorPrivate>(this);
    const auto postHandler =
        [this, guard]()
        {
            if (m_httpClient->failed())
            {
                finalize(Result::ConnectionError);
                return;
            }

            const auto response = m_httpClient->response();
            if (!response)
            {
                finalize(Result::UnspecifiedError);
                return;
            }

            switch(response->statusLine.statusCode)
            {
                case nx_http::StatusCode::ok:
                    finalize(Result::Success);
                    break;
                case nx_http::StatusCode::internalServerError:
                    finalize(Result::ServerError);
                    break;
                case nx_http::StatusCode::badRequest:
                {
                    finalize(Result::DeactivationError,
                        extractErrors(m_httpClient->fetchMessageBodyBuffer()));
                    break;
                }
                default:
                    finalize(Result::UnspecifiedError);
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
    m_httpClient->doPost(kDeactivateLicenseUrl, kJsonContentType, body);
}

void LicenseDeactivatorPrivate::finalize(
    Result result,
    const LicenseErrorHash& errors)
{
    const auto deleteLaterGuard = QnRaiiGuard::createDestructible(
        [this]() { deleteLater(); });

    if (thread() != QThread::currentThread())
        NX_EXPECT(false, "Deactivation handler is trying to be called in wrong thread");
    else if (m_handler)
        m_handler(result, errors);
}

}

//------------------------------------------------------------------------------------------------

namespace nx {
namespace client {
namespace desktop {
namespace helpers {
namespace license {

void Deactivator::deactivateAsync(
    const QnLicenseList& licenses,
    const Handler& completionHandler,
    QObject* parent)
{
    // Deactivator will delete himself when after operation complete
    new LicenseDeactivatorPrivate(licenses, completionHandler, parent);
}

QString Deactivator::resultDescription(Result result, int licensesCount)
{
    switch(result)
    {
        case Result::Success:
            return tr("License(s) deactivated", nullptr, licensesCount);
        case Result::UnspecifiedError:
            return tr("Unspecified error");
        case Result::ConnectionError:
            return tr("Request failed. Check internet connection");
        case Result::ServerError:
            return tr("License server error");
        case Result::DeactivationError:
            return tr("License(s) deactivation error", nullptr, licensesCount);
        default:
            NX_EXPECT(false, "Unhandled switch value");
            return QString();
    }
}

QString Deactivator::errorDescription(ErrorCode error)
{
    switch(error)
    {
        case ErrorCode::NoError:
            return QString();
        case ErrorCode::UnknownError:
            return tr("Unknown error");
        case ErrorCode::LicenseDeactivatedAlready:
            return tr("License is inactive");
        case ErrorCode::LimitExceeded:
            return tr("Limit exceeded");
        default:
            NX_EXPECT(false, "We don't excpect to be here");
            return QString();
    }
}

} // license namespace
} // helpers namespace
} // desktop namespace
} // client namespace
} // nx namespace
