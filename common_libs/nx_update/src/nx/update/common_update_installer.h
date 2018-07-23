#pragma once

#include <functional>
#include <nx/update/detail/zip_extractor.h>
#include <nx/vms/api/data/system_information.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {

enum class NX_UPDATE_API PrepareResult
{
    idle,
    ok,
    inProgress,
    corruptedArchive,
    noFreeSpace,
    cleanTemporaryFilesError,
    updateContentsError,
    unknownError,
};

class NX_UPDATE_API CommonUpdateInstaller
{
public:
    void prepareAsync(const QString& path);
    bool install();
    ~CommonUpdateInstaller();
    void stopSync();
    PrepareResult state() const;

private:
    update::detail::ZipExtractor m_extractor;
    mutable QnMutex m_mutex;
    QnWaitCondition m_condition;
    mutable QString m_version;
    mutable QString m_executable;
    mutable PrepareResult m_state = PrepareResult::idle;

    virtual QString dataDirectoryPath() const = 0;
    virtual bool initializeUpdateLog(const QString& targetVersion, QString* logFileName) const = 0;

    void setState(PrepareResult result);
    void setStateLocked(PrepareResult result);
    bool cleanInstallerDirectory();
    QVariantMap updateInformation(const QString& outputPath) const;
    vms::api::SystemInformation systemInformation() const;
    bool checkExecutable(const QString& executableName) const;
    QString installerWorkDir() const;
    PrepareResult checkContents(const QString& outputPath) const;
};

} // namespace nx
