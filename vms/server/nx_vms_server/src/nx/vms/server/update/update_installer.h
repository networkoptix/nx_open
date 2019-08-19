#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/update/detail/zip_extractor.h>
#include <nx/vms/api/data/system_information.h>
#include <nx/vms/server/server_module_aware.h>

class QTimer;

struct QnAuthSession;

namespace nx::vms::server {

class UpdateInstaller: public ServerModuleAware
{
public:
    enum class State
    {
        idle,
        ok,
        inProgress,
        noFreeSpace,
        noFreeSpaceToInstall,
        brokenZip,
        wrongDir,
        cantOpenFile,
        otherError,
        stopped,
        busy,
        installing,
        cleanTemporaryFilesError,
        updateContentsError,
        unknownError,
    };

    UpdateInstaller(QnMediaServerModule* serverModule);
    ~UpdateInstaller();

    void prepareAsync(const QString& path);
    void install(const QnAuthSession& authInfo);
    void installDelayed();
    void stopSync(bool clearAndReset);
    void recheckFreeSpaceForInstallation();
    State state() const;

    bool checkFreeSpace(const QString& path, qint64 bytes) const;
    bool checkFreeSpaceForInstallation() const;

    QString dataDirectoryPath() const;
    QString component() const;
    int64_t freeSpace(const QString& path) const;

private:
    bool initializeUpdateLog(const QString& targetVersion, QString* logFileName) const;

    void setState(State state);
    void setStateLocked(State state);
    bool cleanInstallerDirectory();
    bool checkExecutable(const QString& executableName) const;
    State checkContents(const QString& outputPath) const;
    QString workDir() const;
    void stopInstallationTimerAsync();

private:
    update::detail::ZipExtractor m_extractor;
    mutable QnMutex m_mutex;
    QnWaitCondition m_condition;
    mutable QString m_version;
    mutable QString m_executable;
    qint64 m_bytesExtracted = 0;
    mutable qint64 m_freeSpaceRequiredToUpdate;
    mutable State m_state = State::idle;
    std::unique_ptr<QTimer> m_installationTimer;
};

} // namespace nx::vms::server
