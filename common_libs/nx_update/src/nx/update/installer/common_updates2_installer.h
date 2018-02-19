#pragma once

#include <nx/update/installer/detail/updates2_installer_base.h>

namespace nx {
namespace update {

class NX_UPDATE_API CommonUpdates2Installer: public detail::Updates2InstallerBase
{
private:
    virtual bool cleanInstallerDirectory() override;
    virtual detail::AbstractZipExtractorPtr createZipExtractor() const override;
    virtual QVariantMap updateInformation() const override;
    virtual QnSystemInformation systemInformation() const override;
    virtual bool checkExecutable(const QString& executableName) const override;
};

} // namespace updates2
} // namespace nx
