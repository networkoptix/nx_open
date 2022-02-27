// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QHash>
#include <QtCore/QCoreApplication>

#include <licensing/license_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/url.h>

namespace nx::vms::client::desktop::license {

struct RequestInfo
{
    QString name;
    QString email;
    QStringList reason;
    QString systemName;
    QString localUser;
};

class Deactivator
{
    Q_DECLARE_TR_FUNCTIONS(Deactivator)

public:
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(ErrorCode,
        noError,
        unknownError,

        // Keys from license server
        keyIsNotActivated,

        keyIsInvalid,
        keyDoesntExist,
        keyIsDisabled,
        keyIsTrial,
        deactivationIsPending,
        invalidHardwareId,
        limitExceeded
    )

    enum class Result
    {
        Success,
        UnspecifiedError,
        ConnectionError,

        ServerError,
        DeactivationError
    };

    using LicenseErrorHash = QHash<QByteArray, ErrorCode>;
    using Handler = std::function<void(Result, const LicenseErrorHash&)>;

    static void deactivateAsync(
        const nx::utils::Url& url,
        const RequestInfo& info,
        const QnLicenseList& licenses,
        const Handler& completionHandler,
        QObject* parent);

    static QString errorDescription(ErrorCode error);
};

using DeactivationErrors = Deactivator::LicenseErrorHash;

} // namespace nx::vms::client::desktop::license
