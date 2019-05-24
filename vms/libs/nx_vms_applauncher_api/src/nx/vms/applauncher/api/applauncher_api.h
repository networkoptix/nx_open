#pragma once

#include <QtCore/QStringList>

#include <nx/utils/software_version.h>

namespace nx::vms::applauncher::api {

/**
 * Name of named pipe, used by applauncher to post tasks to the queue and by client to use the
 * compatibility mode
 */
NX_VMS_APPLAUNCHER_API_API QString launcherPipeName();

enum class TaskType
{
    startApplication,
    quit,
    installZip,
    isVersionInstalled,
    getInstalledVersions,
    addProcessKillTimer,
    startZipInstallation,
    checkZipProgress,
    invalidTaskType
};

NX_VMS_APPLAUNCHER_API_API TaskType deserializeTaskType(const QByteArray& data);
NX_VMS_APPLAUNCHER_API_API QByteArray serializeTaskType(TaskType value);
NX_VMS_APPLAUNCHER_API_API QString toString(TaskType value);

enum class ResultType
{
    ok,
    /** Failed to connect to applauncher. */
    connectError,
    /** Requested client version is not installed. */
    versionNotInstalled,
    /** Requested client version is installed. */
    alreadyInstalled,
    /** Requested version has invalid format. */
    invalidVersionFormat,
    notFound,
    badResponse,
    ioError,
    /** Not enough space to install client package. */
    notEnoughSpace,
    /** Zip with update data is broken and can not be installed. */
    brokenPackage,
    /** Applauncher is still unpacking zip file. */
    unpackingZip,
    /** Applauncher can not start another async task right now. */
    busy,
    /** Unknown error. */
    otherError
};

NX_VMS_APPLAUNCHER_API_API ResultType deserializeResultType(const QByteArray& str);
NX_VMS_APPLAUNCHER_API_API QByteArray serializeResultType(ResultType val);
NX_VMS_APPLAUNCHER_API_API QString toString(ResultType value);

class NX_VMS_APPLAUNCHER_API_API BaseTask
{
public:
    const TaskType type;

    BaseTask(TaskType type): type(type) {}
    virtual ~BaseTask() = default;

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
};

//* Parses serializedTask header, creates the corresponding object (*ptr) and calls deserialize. */
NX_VMS_APPLAUNCHER_API_API bool deserializeTask(const QByteArray& serializedTask, BaseTask** ptr);

/**
 * Start the specified application version.
 */
class NX_VMS_APPLAUNCHER_API_API StartApplicationTask: public BaseTask
{
public:
    nx::utils::SoftwareVersion version;
    //* Command-line params to pass to application instance. */
    QStringList arguments;

    StartApplicationTask(): BaseTask(TaskType::startApplication) {}
    StartApplicationTask(const nx::utils::SoftwareVersion& version, const QStringList& arguments):
        BaseTask(TaskType::startApplication),
        version(version),
        arguments(arguments)
    {
    }

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
};

/**
 * Quit the applauncher process.
 */
class NX_VMS_APPLAUNCHER_API_API QuitTask: public BaseTask
{
public:
    QuitTask(): BaseTask(TaskType::quit) {}
};

/**
 * Install Client from the given ZIP archive.
 */
class NX_VMS_APPLAUNCHER_API_API InstallZipTask: public BaseTask
{
public:
    nx::utils::SoftwareVersion version;
    QString zipFileName;

    InstallZipTask(): BaseTask(TaskType::installZip) {}
    InstallZipTask(const nx::utils::SoftwareVersion& version, const QString& zipFileName):
        BaseTask(TaskType::installZip),
        version(version),
        zipFileName(zipFileName)
    {
    }

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
};

/**
 * A command to start installation of a zip package. It will run asynchronously. Client should send
 * InstallZipCheckStatus request to get status of zip installation.
 */
class NX_VMS_APPLAUNCHER_API_API InstallZipTaskAsync: public BaseTask
{
public:
    nx::utils::SoftwareVersion version;
    QString zipFileName;

    InstallZipTaskAsync(): BaseTask(TaskType::startZipInstallation) {}
    InstallZipTaskAsync(const nx::utils::SoftwareVersion& version, const QString& zipFileName):
        BaseTask(TaskType::startZipInstallation),
        version(version),
        zipFileName(zipFileName)
    {
    }

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
};

/**
 * A command to chesk status of installation of a zip package.
 */
class NX_VMS_APPLAUNCHER_API_API InstallZipCheckStatus: public BaseTask
{
public:
    InstallZipCheckStatus(): BaseTask(TaskType::checkZipProgress) {}
};

/*
 * Check, if the specified version is installed.
 */
class NX_VMS_APPLAUNCHER_API_API IsVersionInstalledRequest: public BaseTask
{
public:
    nx::utils::SoftwareVersion version;

    IsVersionInstalledRequest(): BaseTask(TaskType::isVersionInstalled) {}

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
};

/*
 * Get a list of all installed versions.
 */
class NX_VMS_APPLAUNCHER_API_API GetInstalledVersionsRequest: public BaseTask
{
public:
    GetInstalledVersionsRequest(): BaseTask(TaskType::getInstalledVersions) {}

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
};

class NX_VMS_APPLAUNCHER_API_API Response
{
public:
    virtual ~Response() = default;

    ResultType result = ResultType::ok;

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
};

class NX_VMS_APPLAUNCHER_API_API IsVersionInstalledResponse: public Response
{
public:
    bool installed = false;

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
};

class NX_VMS_APPLAUNCHER_API_API GetInstalledVersionsResponse: public Response
{
public:
    QList<nx::utils::SoftwareVersion> versions;

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
};

class NX_VMS_APPLAUNCHER_API_API AddProcessKillTimerRequest: public BaseTask
{
public:
    qint64 processId = 0;
    quint32 timeoutMillis = 0;

    AddProcessKillTimerRequest(): BaseTask(TaskType::addProcessKillTimer) {}

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
};

class NX_VMS_APPLAUNCHER_API_API AddProcessKillTimerResponse: public Response
{
};

} // namespace nx::vms::applauncher::api
