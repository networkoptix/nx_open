// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_helpers.h"

#include <chrono>
#include <unordered_map>

#include <QtCore/QThread>

#include <licensing/license.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/reflect/from_string.h>
#include <nx/reflect/json.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/license/usage_helper.h>
#include <utils/common/delayed.h>

namespace detail {

struct LicenseData
{
    QString key;
    QString hwid;
};

NX_REFLECTION_INSTRUMENT(LicenseData, (key)(hwid))
using LicenseDataList = QList<LicenseData>;

struct Request
{
    nx::vms::client::desktop::license::RequestInfo deactivationInfo;
    LicenseDataList licenses;
};
NX_REFLECTION_INSTRUMENT(Request, (licenses)(deactivationInfo))

struct LicenseStatus
{
    QString code;
    QString text;
};
NX_REFLECTION_INSTRUMENT(LicenseStatus, (code)(text))

using LicenseStatusHash = std::unordered_map<QString, LicenseStatus>;

struct LicenseStatusData
{
    LicenseStatusHash licenseWarnings;
    LicenseStatusHash licenseErrors;
};
NX_REFLECTION_INSTRUMENT(LicenseStatusData, (licenseWarnings)(licenseErrors))

struct ErrorReply
{
    QString error;
    LicenseStatusData errors;
};
NX_REFLECTION_INSTRUMENT(ErrorReply, (error)(errors))

} // namespace detail

namespace {

using namespace std::chrono;

static constexpr milliseconds kRequestTimeout = 30s;

using namespace nx::vms::client::desktop::license;
using ErrorCode = Deactivator::ErrorCode;
using LicenseErrorMap = Deactivator::LicenseErrorHash;

ErrorCode toErrorCode(const QString& text)
{
    return nx::reflect::fromString<ErrorCode>(text.toStdString(), ErrorCode::unknownError);
}

LicenseErrorMap extractErrors(const std::string_view& messageBody)
{
    const auto[errorReply, ok] = nx::reflect::json::deserialize<detail::ErrorReply>(messageBody);
    auto& errors = errorReply.errors.licenseErrors;

    LicenseErrorMap result;
    for (const auto& [key, value]: errors)
        result.insert(key.toLatin1(), toErrorCode(value.code));

    return result;
}

class LicenseDeactivatorPrivate: public QObject
{
    using base_type = QObject;
    using Handler = Deactivator::Handler;
    using Result = Deactivator::Result;

public:
    LicenseDeactivatorPrivate(
        const nx::utils::Url& url,
        const RequestInfo& info,
        const QnLicenseList& licenses,
        const Handler& handler,
        QObject* parent);

private:
    void finalize(
        Result result,
        const LicenseErrorMap& errors = LicenseErrorMap());

private:
    const nx::network::http::AsyncHttpClientPtr m_httpClient;
};

LicenseDeactivatorPrivate::LicenseDeactivatorPrivate(
    const nx::utils::Url& url,
    const RequestInfo& info,
    const QnLicenseList& licenses,
    const Handler& handler,
    QObject* parent)
    :
    base_type(parent),
    m_httpClient(nx::network::http::AsyncHttpClient::create(nx::network::ssl::kDefaultCertificateCheck))
{
    m_httpClient->setSendTimeoutMs(kRequestTimeout.count());
    m_httpClient->setResponseReadTimeoutMs(kRequestTimeout.count());

    const auto finalize =
        [this, handler](Result result, const LicenseErrorMap& errors = LicenseErrorMap())
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
                case nx::network::http::StatusCode::ok:
                    finalize(Result::Success);
                    break;
                case nx::network::http::StatusCode::internalServerError:
                    finalize(Result::ServerError);
                    break;
                case nx::network::http::StatusCode::badRequest:
                {
                    finalize(Result::DeactivationError,
                        extractErrors(m_httpClient->fetchMessageBodyBuffer().takeStdString()));
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
        [guard, handler, postHandler](nx::network::http::AsyncHttpClientPtr /*client*/)
        {
            if (guard && handler)
                executeLaterInThread(postHandler, guard->thread());
        };

    connect(m_httpClient.get(), &nx::network::http::AsyncHttpClient::done, this, threadSafeHandler,
        Qt::DirectConnection);

    detail::Request request;
    request.deactivationInfo = info;
    for (const auto& license: licenses)
        request.licenses.append({nx::toString(license->key()), license->hardwareId()});

    m_httpClient->doPost(
        url,
        nx::network::http::header::ContentType::kJson,
        nx::reflect::json::serialize(request));
}

} // namespace

//------------------------------------------------------------------------------------------------

namespace nx::vms::client::desktop::license {

NX_REFLECTION_INSTRUMENT(RequestInfo, (name)(email)(reason)(systemName)(localUser))

void Deactivator::deactivateAsync(
    const nx::utils::Url& url,
    const RequestInfo& info,
    const QnLicenseList& licenses,
    const Handler& completionHandler,
    QObject* parent)
{
    // Deactivator will delete himself when operation complete
    new LicenseDeactivatorPrivate(url, info, licenses, completionHandler, parent);
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
            return tr("Hardware ID of Server with"
                    " this license does not match Hardware ID on which license was activated.");
        case ErrorCode::limitExceeded:
            return tr("Number of deactivations exceeded limit for this license.");
    }

    NX_ASSERT(false, "We don't expect to be here");
    return QString();
}

} // namespace nx::vms::client::desktop::license
