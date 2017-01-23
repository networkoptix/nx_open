#pragma once

#include <chrono>
#include <memory>
#include <QtCore>
#include <core/resource/resource_fwd.h>
#include <server/server_globals.h>

class QnStorageManager;

namespace nx {
namespace caminfo {

class ComposerHandler
{
public:
    virtual ~ComposerHandler() {}
    virtual QString name() const = 0;
    virtual QString model() const = 0;
    virtual QString groupId() const = 0;
    virtual QString groupName() const = 0;
    virtual QString url() const = 0;
    virtual QList<QPair<QString, QString>> properties() const = 0;
    virtual bool hasCamera() const = 0;
};

class Composer
{
    static constexpr const char* const kArchiveCameraUrlKey = "cameraUrl";
    static constexpr const char* const kArchiveCameraNameKey = "cameraName";
    static constexpr const char* const kArchiveCameraModelKey = "cameraModel";
    static constexpr const char* const kArchiveCameraGroupIdKey = "groupId";
    static constexpr const char* const kArchiveCameraGroupNameKey = "groupName";

public:
    QByteArray make(ComposerHandler* composerHandler);

private:
    QString formatString(const QString& source) const;
    void printKeyValue(const QString& key, const QString& value);
    void printProperties();

private:
    std::unique_ptr<QTextStream> m_stream;
    ComposerHandler* m_handler;
};

class WriterHandler
{
public:
    virtual ~WriterHandler() {}
    virtual QStringList storagesUrls() const = 0;
    virtual QStringList camerasIds(QnServer::ChunksCatalog) const = 0;
    virtual bool needStop() const = 0;
    virtual bool replaceFile(const QString& path, const QByteArray& data) = 0;
    virtual ComposerHandler* composerHandler(const QString& cameraId) = 0;
};

const std::chrono::minutes kWriteInfoFilesInterval(5);

class Writer
{
public:
    Writer(WriterHandler* writeHandler, std::chrono::milliseconds writeInterval = kWriteInfoFilesInterval);
    void write();

private:
    void writeInfoIfNeeded(const QString& infoFilePath, const QByteArray& infoFileData);
    static QString makeFullPath(
        const QString& storageUrl,
        QnServer::ChunksCatalog catalog,
        const QString& cameraId);

protected:
    void setWriteInterval(std::chrono::milliseconds interval);

private:
    WriterHandler* m_handler;
    std::chrono::milliseconds m_writeInterval;
    std::chrono::time_point<std::chrono::steady_clock> m_lastWriteTime;
    QMap<QString, QByteArray> m_infoPathToCameraInfo;
    Composer m_composer;
};

class ServerHandler:
    public WriterHandler,
    public ComposerHandler
{
public: // WriterHandler
    ServerHandler(QnStorageManager* storageManager);
    virtual QStringList storagesUrls() const override;
    virtual QStringList camerasIds(QnServer::ChunksCatalog) const override;
    virtual bool needStop() const override;
    virtual bool replaceFile(const QString& path, const QByteArray& data) override;
    virtual ComposerHandler* composerHandler(const QString& cameraId) override;

public: // ComposerHandler
    virtual QString name() const override;
    virtual QString model() const override;
    virtual QString groupId() const override;
    virtual QString groupName() const override;
    virtual QString url() const override;
    virtual QList<QPair<QString, QString>> properties() const override;
    virtual bool hasCamera() const override;

private:
    QnStorageManager* m_storageManager;
    QnSecurityCamResourcePtr m_camera;
};

class Reader
{

};

}
}
