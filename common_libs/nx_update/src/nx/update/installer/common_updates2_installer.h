#pragma once

#include <nx/update/installer/detail/updates2_installer_base.h>
#include <nx/vms/api/data/system_information.h>

namespace nx {
namespace update {

class NX_UPDATE_API CommonUpdates2Installer: public installer::detail::Updates2InstallerBase
{
private:
    virtual bool cleanInstallerDirectory() override;
    virtual installer::detail::AbstractZipExtractorPtr createZipExtractor() const override;
    virtual QVariantMap updateInformation(const QString& outputPath) const override;
    virtual vms::api::SystemInformation systemInformation() const override;
    virtual bool checkExecutable(const QString& executableName) const override;
};

} // namespace updates2
} // namespace nx
