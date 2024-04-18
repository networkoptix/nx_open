// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMultiMap>

#include <nx/vms/update/update_check.h>

namespace nx::vms::common::update {

static const QString kTargetKey = "target";
static const QString kInstalledKey = "installed";

struct NX_VMS_COMMON_API TargetVersionParams {};
struct NX_VMS_COMMON_API InstalledVersionParams {};

using UpdateInfoParamsBase = std::variant<
    TargetVersionParams,
    InstalledVersionParams,
    nx::vms::update::LatestVmsVersionParams,
    nx::vms::update::LatestDesktopClientVersionParams,
    nx::vms::update::CertainVersionParams>;

class NX_VMS_COMMON_API UpdateInfoParams: public UpdateInfoParamsBase
{
public:
    using UpdateInfoParamsBase::UpdateInfoParamsBase;

    UpdateInfoParams(const nx::vms::update::PublicationInfoParams& params);

    std::optional<vms::update::PublicationInfoParams> toPublicationInfoParams() const;
    static UpdateInfoParams fromPublicationInfoParams(
        const vms::update::PublicationInfoParams& params);

    QMultiMap<QString, QString> toRestParams() const;
    static UpdateInfoParams fromRestParams(const QMultiMap<QString, QString>& params);
};

} // namespace nx::vms::common::update
