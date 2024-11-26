// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx {
namespace vms {
namespace api {

// TODO: Find out how it relates to nx::utils::OsInfo.
class NX_VMS_API OsInformation
{
    Q_GADGET
    Q_PROPERTY(QString arch MEMBER arch CONSTANT)
    Q_PROPERTY(QString platform MEMBER platform CONSTANT)
    Q_PROPERTY(QString modification MEMBER modification CONSTANT)

public:
    OsInformation() = default;
    OsInformation(
        const QString& platform,
        const QString& arch,
        const QString& modification = {});
    OsInformation(const QString& infoString);

    Q_INVOKABLE QString toString() const;
    Q_INVOKABLE bool isValid() const;

    QString arch;
    QString platform;
    QString modification;

    static OsInformation fromBuildInfo();

    static Q_INVOKABLE QString currentSystemRuntime();
};
#define OsInformation_Fields (arch)(platform)(modification)
QN_FUSION_DECLARE_FUNCTIONS(OsInformation,
    (hash)(ubjson)(json)(xml)(csv_record),
    NX_VMS_API)
NX_REFLECTION_INSTRUMENT(OsInformation, OsInformation_Fields);

} // namespace api
} // namespace vms
} // namespace nx
