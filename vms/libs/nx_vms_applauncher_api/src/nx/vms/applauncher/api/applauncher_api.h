// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/software_version.h>

namespace nx::vms::applauncher::api {

/**
 * Name of named pipe, used by applauncher to post tasks to the queue and by client to use the
 * compatibility mode
 */
NX_VMS_APPLAUNCHER_API_API QString launcherPipeName();

NX_REFLECTION_ENUM_CLASS(TaskType,
    run,
    quit,
    installZip,
    isVersionInstalled,
    getInstalledVersions,
    addProcessKillTimer,
    startZipInstallation,
    checkZipProgress,
    pingApplauncher,
    unknown
);

NX_REFLECTION_ENUM_CLASS(ResultType,
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
);

class NX_VMS_APPLAUNCHER_API_API BaseTask
{
public:
    const TaskType type;

    BaseTask(TaskType type): type(type) {}
    virtual ~BaseTask() = default;

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
};

class NX_VMS_APPLAUNCHER_API_API Response
{
public:
    virtual ~Response() = default;

    ResultType result = ResultType::ok;

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
};

/**
 * Start the specified application version.
 */
class NX_VMS_APPLAUNCHER_API_API StartApplicationTask: public BaseTask
{
public:
    nx::utils::SoftwareVersion version;
    //* Command-line params to pass to application instance. */
    QStringList arguments;

    StartApplicationTask(): BaseTask(TaskType::run) {}
    StartApplicationTask(const nx::utils::SoftwareVersion& version, const QStringList& arguments):
        BaseTask(TaskType::run),
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
    nx::utils::SoftwareVersion version;

    InstallZipCheckStatus(): BaseTask(TaskType::checkZipProgress) {}

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
};

class NX_VMS_APPLAUNCHER_API_API InstallZipCheckStatusResponse: public Response
{
public:
    /** Number of bytes extracted. */
    quint64 extracted = 0;
    /** Total byte size of extracted data. */
    quint64 total = 0;

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
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

class NX_VMS_APPLAUNCHER_API_API IsVersionInstalledResponse: public Response
{
public:
    bool installed = false;

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

class NX_VMS_APPLAUNCHER_API_API PingRequest: public BaseTask
{
public:
    quint64 pingId = 0;
    /** Stamp for the time request was generated and sent. */
    quint64 pingStamp = 0;

    PingRequest(): BaseTask(TaskType::pingApplauncher) {}

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
};

class NX_VMS_APPLAUNCHER_API_API PingResponse: public Response
{
public:
    quint64 pingId = 0;
    /** Stamp for the time request was generated and sent. It was taken from PingRequest. */
    quint64 pingRequestStamp = 0;
    /** Stamp for the time response was generated and sent. */
    quint64 pingResponseStamp = 0;

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
};


} // namespace nx::vms::applauncher::api
