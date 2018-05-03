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
        installing,
        error,
    };
    Q_ENUM(StatusCode)

    QnUuid serverId;
    StatusCode state = StatusCode::notAvailable;
    QString message;
    double progress = 0.0;

    Updates2StatusData() = default;
    Updates2StatusData(
        const QnUuid& serverId,
        StatusCode state,
        QString message = QString(),
        double progress = 0.0)
        :
        serverId(serverId),
        state(state),
        message(std::move(message)),
        progress(progress)
    {}
};

#define Updates2StatusData_Fields (serverId)(state)(message)(progress)
QN_FUSION_DECLARE_FUNCTIONS(Updates2StatusData::StatusCode, (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Updates2StatusData), (json)(eq))

} // namespace api
} // namespace nx
