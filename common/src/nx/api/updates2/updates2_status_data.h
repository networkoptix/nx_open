#pragma once

#include <QtCore>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

struct Updates2StatusData
{
    Q_GADGET

public:
    enum class StatusCode
    {
        checking,
        available,
        notAvailable,
        downloading,
        preparing,
        readyToInstall,
        error,
    };
    Q_ENUM(StatusCode)

    QnUuid serverId;
    StatusCode status = StatusCode::notAvailable;
    QString message;
    double progress = 0.0;

    Updates2StatusData() = default;
    Updates2StatusData(
        const QnUuid& serverId,
        StatusCode status,
        QString message = QString(),
        double progress = 0.0)
        :
        serverId(serverId),
        status(status),
        message(std::move(message)),
        progress(progress)
    {}
};

#define Updates2StatusData_Fields (serverId)(status)(message)(progress)
QN_FUSION_DECLARE_FUNCTIONS(Updates2StatusData::StatusCode, (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Updates2StatusData), (json))

} // namespace api
} // namespace nx
