#pragma once

#include <functional>

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QCoreApplication>

#include <licensing/license_fwd.h>

namespace nx {
namespace client {
namespace desktop {
namespace license {

class Deactivator
{
    Q_DECLARE_TR_FUNCTIONS(Deactivator)

public:
    enum class ErrorCode
    {
        NoError,
        UnknownError,

        LicenseDeactivatedAlready,
        LimitExceeded
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
        const QnLicenseList& licenses,
        const Handler& completionHandler,
        QObject* parent);

    static QString resultDescription(Result result, int licensesCount);

    static QString errorDescription(ErrorCode error);

};

} // namespace license
} // namespace desktop
} // namespace client
} // namespace nx
