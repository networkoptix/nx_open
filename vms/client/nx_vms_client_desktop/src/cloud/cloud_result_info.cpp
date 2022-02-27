// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_result_info.h"

#include <nx/branding.h>

using ResultCode = nx::cloud::db::api::ResultCode;

QnCloudResultInfo::QnCloudResultInfo(ResultCode code):
    m_text(toString(code))
{
}

QnCloudResultInfo::operator QString() const
{
    return m_text;
}

QString QnCloudResultInfo::toString(ResultCode code)
{
    switch (code)
    {
        case ResultCode::ok:
        case ResultCode::partialContent:
            return tr("Successful.");

        case ResultCode::notAuthorized:
            return tr("Invalid login or password.");

        case ResultCode::forbidden:
            return tr("Requested operation is not allowed with provided credentials.");

        case ResultCode::accountNotActivated:
            return tr("This account is not activated. Please check your email.");

        case ResultCode::accountBlocked:
            return tr("Too many attempts. Try again in a minute.");

        case ResultCode::networkError:
            return tr("Network error. Please check your Internet connection and try again.");

        case ResultCode::badUsername:
            return tr("Invalid login.");

        case ResultCode::serviceUnavailable:
        case ResultCode::retryLater:
            return tr("%1 is temporary unavailable. Please try again later.",
                "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName());

        case ResultCode::credentialsRemovedPermanently:
            return tr("Credentials are no longer valid.");

        default:
            return tr("Internal %1 error. Please contact support team.",
                "%1 is the cloud name (like Nx Cloud)")
                .arg(nx::branding::cloudName());
    }
}

