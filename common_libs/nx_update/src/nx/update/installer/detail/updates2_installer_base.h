#pragma once

#include <nx/update/installer/detail/abstract_updates2_installer.h>
#include <nx/update/installer/detail/abstract_zip_extractor.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <utils/common/system_information.h>

namespace nx {
namespace update {
namespace installer {
namespace detail {

class NX_UPDATE_API Updates2InstallerBase: public AbstractUpdates2Installer
{
public:
    virtual void prepareAsync(const QString& path, PrepareUpdateCompletionHandler handler) override;
    virtual bool install() override;
    virtual ~Updates2InstallerBase() override;
    virtual void stopSync() override;

protected:
    QString installerWorkDir() const;
    PrepareResult checkContents(const QString& outputPath) const;

private:
    AbstractZipExtractorPtr m_extractor;
    QnMutex m_mutex;
    QnWaitCondition m_condition;
    bool m_running = false;
    mutable QString m_version;
    mutable QString m_executable;

    // These below should be overriden in Server{Client}Updates2Installer
    virtual QString dataDirectoryPath() const = 0;
    virtual bool initializeUpdateLog(const QString& targetVersion, QString* logFileName) const = 0;

    // These below should be overriden in CommonUpdates2Installer
    virtual bool cleanInstallerDirectory() = 0;
    virtual AbstractZipExtractorPtr createZipExtractor() const = 0;
    virtual QVariantMap updateInformation(const QString& outputPath) const = 0;
    virtual QnSystemInformation systemInformation() const = 0;
    virtual bool checkExecutable(const QString& executableName) const = 0;
};

} // namespace detail
} // namespace installer
} // namespace update
} // namespace nx
