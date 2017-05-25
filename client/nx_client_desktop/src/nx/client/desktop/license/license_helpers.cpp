#include "license_helpers.h"

#include <QtCore/QThread>

#include <nx/network/http/asynchttpclient.h>
#include <nx/fusion/serialization_format.h>
#include <nx/utils/raii_guard.h>

#include <rest/server/json_rest_result.h>
#include <utils/common/delayed.h>
#include <utils/common/connective.h>

#include <licensing/license.h>

#include <nx/fusion/model_functions.h>

namespace detail {

struct LicenseData
{
    QString key;
    QString hwid;
};
QN_FUSION_DECLARE_FUNCTIONS(LicenseData, (json))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LicenseData, (json), (key)(hwid))

using LicenseDataList = QList<LicenseData>;

struct Licenses
{
    LicenseDataList licenses;
};
QN_FUSION_DECLARE_FUNCTIONS(Licenses, (json))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Licenses, (json), (licenses))

struct LicenseStatus
{
    QString code;
    QString text;
};
QN_FUSION_DECLARE_FUNCTIONS(LicenseStatus, (json))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LicenseStatus, (json), (code)(text))

using LicenseStatusHash = QHash<QString, LicenseStatus>;

struct LicenseStatusData
{
    LicenseStatusHash licenseWarnings;
    LicenseStatusHash licenseErrors;
};
QN_FUSION_DECLARE_FUNCTIONS(LicenseStatusData, (json))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LicenseStatusData, (json), (licenseWarnings)(licenseErrors))

struct ErrorReply
{
    QString error;
    LicenseStatusData errors;
};
QN_FUSION_DECLARE_FUNCTIONS(ErrorReply, (json))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ErrorReply, (json), (error)(errors))

}

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::client::desktop::license::Deactivator, ErrorCode)

namespace {

static const auto kDeactivateLicenseUrl =
    QUrl::fromUserInput(lit("http://nxlicensed.hdw.mx/nxlicensed/api/v1/deactivate"));

using namespace nx::client::desktop::license;
using ErrorCode = Deactivator::ErrorCode;
using LicenseErrorHash = Deactivator::LicenseErrorHash;

ErrorCode toErrorCode(const QString& text)
{
    return QnLexical::deserialized(text, ErrorCode::UnknownError);
}

LicenseErrorHash extractErrors(const QByteArray& messageBody)
{
    const auto errorReply = QJson::deserialized(messageBody, detail::ErrorReply());
    auto& errors = errorReply.errors.licenseErrors;

    LicenseErrorHash result;
    for (const auto& key: errors.keys())
        result.insert(key.toLatin1(), toErrorCode(errors.value(key).code));

    return result;
}

class LicenseDeactivatorPrivate: public Connective<QObject>
{
    using base_type = Connective<QObject>;
    using Handler = Deactivator::Handler;
    using Result = Deactivator::Result;

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
};

LicenseDeactivatorPrivate::LicenseDeactivatorPrivate(
    const QnLicenseList& licenses,
    const Handler& handler,
    QObject* parent)
    :
    base_type(parent),
    m_httpClient(nx_http::AsyncHttpClient::create())
{
    const auto finalize =
        [this, handler](Result result, const LicenseErrorHash& errors = LicenseErrorHash())
        {
            deleteLater();
            if (handler)
                handler(result, errors);
        };

    if (licenses.isEmpty())
    {
        finalize(Result::Success);
        return;
    }

    const auto guard = QPointer<LicenseDeactivatorPrivate>(this);
    const auto postHandler =
        [this, guard, finalize]()
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

    /**
      * We have to use thread safe handler insted of queued connection
      * becase of strong recoomendation in http client header
      */
    const auto threadSafeHandler =
        [this, guard, handler, postHandler](nx_http::AsyncHttpClientPtr /*client*/)
        {
            if (guard && handler)
                executeDelayed(postHandler, 0, guard->thread());
        };

    connect(m_httpClient.get(), &nx_http::AsyncHttpClient::done, this, threadSafeHandler,
        Qt::DirectConnection);

    detail::Licenses data;
    for (const auto& license: licenses)
        data.licenses.append({::toString(license->key()), license->hardwareId()});

    static const QByteArray kJsonContentType(
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat));

    m_httpClient->doPost(kDeactivateLicenseUrl, kJsonContentType, QJson::serialized(data));
}

}

//------------------------------------------------------------------------------------------------

namespace nx {
namespace client {
namespace desktop {
namespace license {

void Deactivator::deactivateAsync(
    const QnLicenseList& licenses,
    const Handler& completionHandler,
    QObject* parent)
{
    // Deactivator will delete himself when operation complete
    new LicenseDeactivatorPrivate(licenses, completionHandler, parent);
}

QString Deactivator::resultDescription(Result result, int licensesCount)
{
    switch (result)
    {
        case Result::Success:
            return tr("Licenses deactivated", "", licensesCount);
        case Result::UnspecifiedError:
            return tr("Unspecified error");
        case Result::ConnectionError:
            return tr("Request failed. Check internet connection");
        case Result::ServerError:
            return tr("License server error");
        case Result::DeactivationError:
            return tr("Licenses deactivation error", "", licensesCount);
    }

    NX_EXPECT(false, "Unhandled switch value");
    return QString();
}

QString Deactivator::errorDescription(ErrorCode error)
{
    switch (error)
    {
        case ErrorCode::NoError:
            return QString();
        case ErrorCode::UnknownError:
            return tr("Unknown error");
        case ErrorCode::keyIsNotActivated:
            return tr("License is inactive");
        case ErrorCode::limitExceeded:
            return tr("Limit exceeded");
        case ErrorCode::keyIsInvalid:
            return tr("Invalid license key");
    }

    NX_EXPECT(false, "We don't expect to be here");
    return QString();
}

} // namespace license
} // namespace desktop
} // namespace client
} // namespace nx
