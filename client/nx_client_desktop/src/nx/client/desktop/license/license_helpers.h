#pragma once

#include <functional>

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QCoreApplication>

#include <licensing/license_fwd.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace client {
namespace desktop {
namespace license {

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
    Q_GADGET
    Q_ENUMS(ErrorCode)

    Q_DECLARE_TR_FUNCTIONS(Deactivator)

public:
    enum class ErrorCode
    {
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
    };

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
        const RequestInfo& info,
        const QnLicenseList& licenses,
        const Handler& completionHandler,
        QObject* parent);

    static QString errorDescription(ErrorCode error);
};

} // namespace license
} // namespace desktop
} // namespace client
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::client::desktop::license::Deactivator::ErrorCode,
    (lexical)(metatype))
