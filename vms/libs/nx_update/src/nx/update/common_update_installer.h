#pragma once

#include <nx/update/detail/zip_extractor.h>
#include <nx/vms/api/data/system_information.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <common/common_module_aware.h>

struct QnAuthSession;

namespace nx {

class CommonUpdateInstaller: public /*mixin*/ QnCommonModuleAware
{
public:
    enum class State
    {
        idle,
        ok,
        inProgress,
        noFreeSpace,
        brokenZip,
        wrongDir,
        cantOpenFile,
        otherError,
        stopped,
        busy,
        cleanTemporaryFilesError,
        updateContentsError,
        unknownError,
    };

    void prepareAsync(const QString& path);
    bool install(const QnAuthSession& authInfo);
    CommonUpdateInstaller(QObject* parent);
    virtual ~CommonUpdateInstaller();
    void stopSync(bool clearAndReset);
    State state() const;

    bool checkFreeSpace(const QString& path, qint64 bytes) const;
    bool checkFreeSpaceForInstallation() const;

    qint64 reservedSpacePadding() const;

    virtual QString dataDirectoryPath() const = 0;
    virtual QString component() const = 0;
    virtual int64_t freeSpace(const QString& path) const = 0;

private:
    update::detail::ZipExtractor m_extractor;
    mutable QnMutex m_mutex;
    QnWaitCondition m_condition;
    mutable QString m_version;
    mutable QString m_executable;
    qint64 m_bytesExtracted = 0;
    mutable qint64 m_freeSpaceRequiredToUpdate;
    mutable CommonUpdateInstaller::State m_state = CommonUpdateInstaller::State::idle;

    virtual bool initializeUpdateLog(const QString& targetVersion, QString* logFileName) const = 0;

    void setState(CommonUpdateInstaller::State result);
    void setStateLocked(CommonUpdateInstaller::State result);
    bool cleanInstallerDirectory();
    bool checkExecutable(const QString& executableName) const;
    CommonUpdateInstaller::State checkContents(const QString& outputPath) const;
    QString workDir() const;
};

} // namespace nx
