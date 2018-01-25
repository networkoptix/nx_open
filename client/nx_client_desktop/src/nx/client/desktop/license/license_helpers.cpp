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
#define LicenseData_Fields (key)(hwid)

using LicenseDataList = QList<LicenseData>;


struct Request
{
    nx::client::desktop::license::RequestInfo deactivationInfo;
    LicenseDataList licenses;
};
#define Request_Fields (licenses)(deactivationInfo)

struct LicenseStatus
{
    QString code;
    QString text;
};
#define LicenseStatus_Fields (code)(text)

using LicenseStatusHash = QHash<QString, LicenseStatus>;

struct LicenseStatusData
{
    LicenseStatusHash licenseWarnings;
    LicenseStatusHash licenseErrors;
};
#define LicenseStatusData_Fields (licenseWarnings)(licenseErrors)

struct ErrorReply
{
    QString error;
    LicenseStatusData errors;
};
#define ErrorReply_Fields (error)(errors)

#define DEACTIVATION_TYPES \
    (LicenseData) \
    (Request) \
    (LicenseStatus) \
    (LicenseStatusData) \
    (ErrorReply)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(DEACTIVATION_TYPES, (json), _Fields)

}

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::client::desktop::license::Deactivator, ErrorCode)

namespace {

static const auto kDeactivateLicenseUrl =
    QUrl::fromUserInput(lit("http://licensing.networkoptix.com/nxlicensed/api/v1/deactivate/"));

using namespace nx::client::desktop::license;
using ErrorCode = Deactivator::ErrorCode;
using LicenseErrorHash = Deactivator::LicenseErrorHash;

ErrorCode toErrorCode(const QString& text)
{
    return QnLexical::deserialized(text, ErrorCode::unknownError);
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
        const RequestInfo& info,
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
    const RequestInfo& info,
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
            if (handler)
                handler(result, errors);

            deleteLater();
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

    detail::Request request;
    request.deactivationInfo = info;
    for (const auto& license: licenses)
        request.licenses.append({::toString(license->key()), license->hardwareId()});

    static const QByteArray kJsonContentType(
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat));

    m_httpClient->doPost(kDeactivateLicenseUrl, kJsonContentType, QJson::serialized(request));
}

}

//------------------------------------------------------------------------------------------------

namespace nx {
namespace client {
namespace desktop {
namespace license {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RequestInfo, (json), (name)(email)(reason)(systemName)(localUser))

void Deactivator::deactivateAsync(
    const RequestInfo& info,
    const QnLicenseList& licenses,
    const Handler& completionHandler,
    QObject* parent)
{
    // Deactivator will delete himself when operation complete
    new LicenseDeactivatorPrivate(info, licenses, completionHandler, parent);
}

QString Deactivator::errorDescription(ErrorCode error)
{
    switch (error)
    {
        case ErrorCode::noError:
            return QString();
        case ErrorCode::unknownError:
            return tr("Unknown error.");
        case ErrorCode::keyDoesntExist:
            return tr("License does not exist.");
        case ErrorCode::keyIsDisabled:
            return tr("License is disabled.");
        case ErrorCode::keyIsNotActivated:
            return tr("License is inactive.");
        case ErrorCode::keyIsInvalid:
            return tr("Invalid license.");
        case ErrorCode::keyIsTrial:
            return tr("License is trial.");
        case ErrorCode::deactivationIsPending:
            return tr("License is in pending deactivation state, but has not been deactivated yet.");
        case ErrorCode::invalidHardwareId:
            return tr("Hardware Id of Server with"
                    " this license does not match Hardware Id on which license was activated.");
        case ErrorCode::limitExceeded:
            return tr("Number of deactivations exceeded limit for this license.");
    }

    NX_EXPECT(false, "We don't expect to be here");
    return QString();
}

} // namespace license
} // namespace desktop
} // namespace client
} // namespace nx
