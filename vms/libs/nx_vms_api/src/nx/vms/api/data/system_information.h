#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace api {

// TODO: #4.1 Replace with SystemInformationNew.
class NX_VMS_API SystemInformation
{
    Q_GADGET
    Q_PROPERTY(QString arch MEMBER arch CONSTANT)
    Q_PROPERTY(QString platform MEMBER platform CONSTANT)
    Q_PROPERTY(QString modification MEMBER modification CONSTANT)
    Q_PROPERTY(QString version MEMBER version CONSTANT)

public:
    SystemInformation() = default;
    SystemInformation(
        const QString& platform,
        const QString& arch,
        const QString& modification = {},
        const QString& version = {});
    SystemInformation(const QString& infoString);

    Q_INVOKABLE QString toString() const;
    Q_INVOKABLE bool isValid() const;

    QString arch;
    QString platform;
    QString modification;
    QString version;

    static Q_INVOKABLE QString runtimeOsVersion();
    static Q_INVOKABLE QString runtimeModification();
    static Q_INVOKABLE QString currentSystemRuntime();
};
#define SystemInformation_Fields \
    (arch)(platform)(modification)(version)

QN_FUSION_DECLARE_FUNCTIONS(SystemInformation,
    (eq)(hash)(ubjson)(json)(xml)(csv_record)(datastream),
    NX_VMS_API)

struct NX_VMS_API SystemInformationNew
{
    QString platform;
    QString variant;
    QString variantVersion; //< TODO: Use nx::utils::SoftwareVersion.

    static SystemInformationNew fromLegacySystemInformation(const SystemInformation& info);
    SystemInformation toLegacySystemInformation() const;
};
#define SystemInformationNew_Fields \
    (platform)(variant)(variantVersion)

QN_FUSION_DECLARE_FUNCTIONS(SystemInformationNew,
    (eq)(hash)(ubjson)(json), NX_VMS_API)

QString toString(const SystemInformationNew& systemInformation);

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::SystemInformation)
