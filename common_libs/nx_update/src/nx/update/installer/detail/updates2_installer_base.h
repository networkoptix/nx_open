#pragma once

#include <nx/update/installer/detail/abstract_updates2_installer.h>
#include <nx/update/installer/detail/abstract_zip_extractor.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {
namespace update {
namespace detail {

class NX_UPDATE_API Updates2InstallerBase: public AbstractUpdates2Installer
{
public:
    virtual void prepareAsync(const QString& path, PrepareUpdateCompletionHandler handler) override;
    virtual void install() override;
    virtual ~Updates2InstallerBase() override;

    void stop();

private:
    AbstractZipExtractorPtr m_extractor;
    QnMutex m_mutex;
    QnWaitCondition m_condition;

    QString installerWorkDir() const;

    // These below should be overriden in Server{Client}Updates2Installer
    virtual QString dataDirectoryPath() const = 0;

    // These below should be overriden in CommonUpdates2Installer
    virtual bool cleanInstallerDirectory() = 0;
    virtual AbstractZipExtractorPtr createZipExtractor() const = 0;
};

} // namespace detail
} // namespace updates2
} // namespace nx
