#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <nx/utils/qnbytearrayref.h>
#include <nx/utils/software_version.h>
#include <nx/utils/app_info.h>

namespace applauncher {
namespace api {

/**
 * Name of named pipe, used by applauncher to post tasks to the queue and by client to use the
 * compatibility mode
 */
NX_VMS_APPLAUNCHER_API_API QString launcherPipeName();

static constexpr int kMaxMessageLengthBytes = 1024 * 64; //64K ought to be enough for anybody

namespace TaskType {
enum Value
{
    startApplication,
    quit,
    installZip,
    isVersionInstalled,
    getInstalledVersions,
    addProcessKillTimer,
    invalidTaskType
};

NX_VMS_APPLAUNCHER_API_API Value fromString(const QnByteArrayConstRef& str);
NX_VMS_APPLAUNCHER_API_API QByteArray toString(Value val);
} // namespace TaskType

class NX_VMS_APPLAUNCHER_API_API BaseTask
{
public:
    const TaskType::Value type;

    BaseTask(TaskType::Value _type);
    virtual ~BaseTask() = default;

    virtual QByteArray serialize() const = 0;
    virtual bool deserialize(const QnByteArrayConstRef& data) = 0;
};

//!Parses \a serializedTask header, creates corresponding object (\a *ptr) and calls deserialize
NX_VMS_APPLAUNCHER_API_API bool deserializeTask(const QByteArray& serializedTask, BaseTask** ptr);

//!Task, sent by application, to start specified application version
class NX_VMS_APPLAUNCHER_API_API StartApplicationTask: public BaseTask
{
public:
    nx::utils::SoftwareVersion version;
    //!Command-line params to pass to application instance
    QStringList appArgs;

    StartApplicationTask();
    StartApplicationTask(const nx::utils::SoftwareVersion& _version,
        const QStringList& _appParams);

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QnByteArrayConstRef& data) override;
};

//!Applauncher process quits running on receiving this task
class NX_VMS_APPLAUNCHER_API_API QuitTask: public BaseTask
{
public:
    QuitTask();

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QnByteArrayConstRef& data) override;
};

class NX_VMS_APPLAUNCHER_API_API InstallZipTask: public BaseTask
{
public:
    nx::utils::SoftwareVersion version;
    QString zipFileName;

    InstallZipTask();
    InstallZipTask(const nx::utils::SoftwareVersion& version, const QString& zipFileName);

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QnByteArrayConstRef& data) override;
};

class NX_VMS_APPLAUNCHER_API_API IsVersionInstalledRequest: public BaseTask
{
public:
    nx::utils::SoftwareVersion version;

    IsVersionInstalledRequest();

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QnByteArrayConstRef& data) override;
};

class NX_VMS_APPLAUNCHER_API_API GetInstalledVersionsRequest
    :
    public BaseTask
{
public:
    GetInstalledVersionsRequest();

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QnByteArrayConstRef &data) override;
};

namespace ResultType {
enum Value
{
    ok,
    //!Failed to connect to applauncher
    connectError,
    versionNotInstalled,
    alreadyInstalled,
    invalidVersionFormat,
    notFound,
    badResponse,
    ioError,
    notEnoughSpace,
    // Zip with update data is broken and can not be installed.
    brokenPackage,
    otherError
};

NX_VMS_APPLAUNCHER_API_API Value fromString(const QnByteArrayConstRef& str);
NX_VMS_APPLAUNCHER_API_API QByteArray toString(Value val);
} // namespace ResultType

class NX_VMS_APPLAUNCHER_API_API Response
{
public:
    virtual ~Response() = default;

    ResultType::Value result = ResultType::ok;

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QnByteArrayConstRef& data);
};

class NX_VMS_APPLAUNCHER_API_API IsVersionInstalledResponse: public Response
{
public:
    bool installed = false;

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QnByteArrayConstRef& data) override;
};

class NX_VMS_APPLAUNCHER_API_API GetInstalledVersionsResponse: public Response
{
public:
    QList<nx::utils::SoftwareVersion> versions;

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QnByteArrayConstRef &data) override;
};

class NX_VMS_APPLAUNCHER_API_API AddProcessKillTimerRequest: public BaseTask
{
public:
    qint64 processID = 0;
    quint32 timeoutMillis = 0;

    AddProcessKillTimerRequest();

    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QnByteArrayConstRef& data) override;
};

class NX_VMS_APPLAUNCHER_API_API AddProcessKillTimerResponse : public Response {};

} // namespace api
} // namespace applauncher
