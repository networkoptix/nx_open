#pragma once

#include <functional>

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
        corruptedArchive,
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
    ~CommonUpdateInstaller();
    void stopSync();
    State state() const;

private:
    update::detail::ZipExtractor m_extractor;
    mutable QnMutex m_mutex;
    QnWaitCondition m_condition;
    mutable QString m_version;
    mutable QString m_executable;
    mutable CommonUpdateInstaller::State m_state = CommonUpdateInstaller::State::idle;

    virtual QString dataDirectoryPath() const = 0;
    virtual bool initializeUpdateLog(const QString& targetVersion, QString* logFileName) const = 0;

    void setState(CommonUpdateInstaller::State result);
    void setStateLocked(CommonUpdateInstaller::State result);
    bool cleanInstallerDirectory();
    QVariantMap updateInformation(const QString& outputPath) const;
    vms::api::SystemInformation systemInformation() const;
    bool checkExecutable(const QString& executableName) const;
    QString installerWorkDir() const;
    CommonUpdateInstaller::State checkContents(const QString& outputPath) const;
};

} // namespace nx
