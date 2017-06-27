#pragma once

#include <functional>
#include <chrono>
#include <memory>
#include <QtCore>
#include <nx/utils/log/log.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/abstract_storage_resource.h>
#include <server/server_globals.h>
#include <nx_ec/data/api_camera_data.h>

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
};

class Composer
{
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
    virtual bool handleFileData(const QString& path, const QByteArray& data) = 0;
    virtual ComposerHandler* composerHandler(const QString& cameraId) = 0;
};

class Writer
{
public:
    Writer(WriterHandler* writeHandler);
    void write();

private:
    void writeInfoIfNeeded(const QString& infoFilePath, const QByteArray& infoFileData);
    bool isWriteNeeded(const QString& infoFilePath, const QByteArray& infoFileData) const;
    static QString makeFullPath(
        const QString& storageUrl,
        QnServer::ChunksCatalog catalog,
        const QString& cameraId);

private:
    WriterHandler* m_handler;
    QMap<QString, QByteArray> m_infoPathToCameraInfo;
    Composer m_composer;
};

class ServerWriterHandler:
    public WriterHandler,
    public ComposerHandler
{
public: // WriterHandler
    ServerWriterHandler(QnStorageManager* storageManager);
    virtual QStringList storagesUrls() const override;
    virtual QStringList camerasIds(QnServer::ChunksCatalog) const override;
    virtual bool needStop() const override;
    virtual bool handleFileData(const QString& path, const QByteArray& data) override;
    virtual ComposerHandler* composerHandler(const QString& cameraId) override;

public: // ComposerHandler
    virtual QString name() const override;
    virtual QString model() const override;
    virtual QString groupId() const override;
    virtual QString groupName() const override;
    virtual QString url() const override;
    virtual QList<QPair<QString, QString>> properties() const override;

private:
    QnStorageManager* m_storageManager;
    QnSecurityCamResourcePtr m_camera;
};

struct ArchiveCameraData
{
    ec2::ApiCameraData coreData;
    ec2::ApiResourceParamDataList properties;
};

typedef std::vector<ArchiveCameraData> ArchiveCameraDataList;

struct ReaderErrorInfo
{
    QString message;
    QnLogLevel severity;
};

class ReaderHandler
{
public:
    virtual ~ReaderHandler() {}
    virtual QnUuid moduleGuid() const = 0;
    virtual QnUuid archiveCamTypeId() const = 0;
    virtual bool isCameraInResPool(const QnUuid& cameraId) const = 0;
    virtual void handleError(const ReaderErrorInfo& errorInfo) const = 0;
};

class Reader
{
    class ParseResult
    {
    public:
        enum class ParseCode
        {
            Ok,
            NoData,
            RegexpFailed,
        };

        ParseResult(ParseCode code): m_code(code) {}
        ParseResult(ParseCode code, const QString& key, const QString& value):
            m_code(code),
            m_key(key),
            m_value(value)
        {}

        QString key() const { return m_key; }
        QString value() const { return m_value; }
        ParseCode code() const {return m_code; }
        QString errorString() const
        {
            switch (m_code)
            {
            case ParseCode::Ok: return QString(); break;
            case ParseCode::NoData: return lit("Line is empty"); break;
            case ParseCode::RegexpFailed: return lit("Line doesn't match parse pattern");
            }

            return "unknown";
        }

    private:
        ParseCode m_code;
        QString m_key;
        QString m_value;
    };

public:
    Reader(ReaderHandler* readerHandler,
           const QnAbstractStorageResource::FileInfo& fileInfo,
           std::function<QByteArray(const QString&)> getFileDataFunc);

    void operator()(ArchiveCameraDataList* outArchiveCameraList);

private:
    bool initArchiveCamData();
    bool cameraAlreadyExists(const ArchiveCameraDataList* camerasList) const;
    bool readFileData();
    bool parseData();
    ParseResult parseLine(const QString& line) const;
    QString infoFilePath() const;
    void addProperty(const ParseResult& result);

private:
    ReaderHandler* m_handler;
    mutable ReaderErrorInfo m_lastError;
    ArchiveCameraData m_archiveCamData;
    QByteArray m_fileData;
    const QnAbstractStorageResource::FileInfo* m_fileInfo;
    std::function<QByteArray(const QString&)> m_getDataFunc;
};

class ServerReaderHandler: public ReaderHandler
{
public:
    virtual QnUuid moduleGuid() const override;
    virtual QnUuid archiveCamTypeId() const override;
    virtual bool isCameraInResPool(const QnUuid& cameraId) const override;
    virtual void handleError(const ReaderErrorInfo& errorInfo) const override;

private:
    mutable QnUuid m_archiveCamTypeId;
};

}
}
