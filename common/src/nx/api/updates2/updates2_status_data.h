#pragma once

#include <QtCore>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/software_version.h>

namespace nx {
namespace api {

struct TargetVersionWithEula
{
    nx::vms::api::SoftwareVersion targetVersion;
    int eulaVersion = -1;
    QString eulaLink;

    TargetVersionWithEula(
        const nx::vms::api::SoftwareVersion& targetVersion,
        int eulaVersion = -1,
        const QString& eulaLink = QString())
        :
        targetVersion(targetVersion),
        eulaVersion(eulaVersion),
        eulaLink(eulaLink)
    {
    }

    TargetVersionWithEula() = default;
};

#define TargetVersionWithEula_Fields (targetVersion)(eulaVersion)(eulaLink)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((TargetVersionWithEula), (json)(ubjson)(eq))

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
    QList<TargetVersionWithEula> targets;
    QString releaseNotesUrl;
    double progress = 0.0;

    Updates2StatusData() = default;
    Updates2StatusData(
        const QnUuid& serverId,
        StatusCode state,
        QString message = QString(),
        const QList<TargetVersionWithEula> targets = QList<TargetVersionWithEula>(),
        const QString& releaseNotesUrl = QString(),
        double progress = 0.0)
        :
        serverId(serverId),
        state(state),
        message(std::move(message)),
        targets(targets),
        releaseNotesUrl(releaseNotesUrl),
        progress(progress)
    {}
};

#define Updates2StatusData_Fields (serverId)(state)(message)(targets)(releaseNotesUrl)(progress)

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Updates2StatusData::StatusCode)
QN_FUSION_DECLARE_FUNCTIONS(Updates2StatusData::StatusCode, (lexical)(numeric))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Updates2StatusData), (json)(ubjson)(eq))

} // namespace api
} // namespace nx
