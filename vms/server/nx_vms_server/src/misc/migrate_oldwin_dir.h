#pragma once

#include "QtCore/QString"

#ifdef Q_OS_WIN32

namespace nx {
namespace misc {

class MigrateDataHandler
{
public:
    virtual ~MigrateDataHandler() {}
    virtual QString currentDataDir() const = 0;
    virtual QString windowsDir() const = 0;
    virtual bool dirExists(const QString& path) const = 0;
    virtual bool makePath(const QString& path) = 0;
    virtual bool moveDir(const QString& source, const QString& target) = 0;
    virtual bool rmDir(const QString& path) = 0;
};

enum class MigrateDataResult
{
    Ok,
    WinDirNotFound,
    NoNeedToMigrate,
    MoveDataFailed
};

MigrateDataResult migrateFilesFromWindowsOldDir(MigrateDataHandler* handler);

class ServerDataMigrateHandler: public MigrateDataHandler
{
public:
    ServerDataMigrateHandler(const QString& dataDir);

    virtual QString currentDataDir() const override;
    virtual QString windowsDir() const override;
    virtual bool dirExists(const QString& path) const override;
    virtual bool makePath(const QString& path) override;
    virtual bool moveDir(const QString& source, const QString& target) override;
    virtual bool rmDir(const QString& path) override;
private:
    QString m_dataDir;
};


} // namespace misc
} // namespace nx
#endif // Q_OS_WIN32
