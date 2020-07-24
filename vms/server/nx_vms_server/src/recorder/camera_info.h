#pragma once

#include <nx/utils/log/log.h>
#include <nx/utils/std/hashes.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/storage_resource.h>
#include <server/server_globals.h>
#include <nx/vms/api/data/camera_data.h>

#include <QtCore>

#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <chrono>
#include <memory>

class QnStorageManager;
class QnMediaServerModule;

namespace nx {
namespace caminfo {

using IdToInfo = std::unordered_map<QString, QByteArray>;

class Writer
{
public:
    Writer(const QnMediaServerModule* serverModule);
    void writeAll(
        const nx::vms::server::StorageResourceList& storages,
        const QStringList& cameraIds);

    void write(
        const nx::vms::server::StorageResourceList& storages,
        const QString& cameraId,
        QnServer::ChunksCatalog quality);

private:
    const QnMediaServerModule* m_serverModule;
    IdToInfo m_idToInfo;
    std::unordered_set<QString> m_paths;

    QnVirtualCameraResourceList getCameras(const QStringList& cameraIds) const;
};

struct ArchiveCameraData
{
    nx::vms::api::CameraData coreData;
    nx::vms::api::ResourceParamDataList properties;
};

typedef std::vector<ArchiveCameraData> ArchiveCameraDataList;

struct ReaderErrorInfo
{
    QString message;
    utils::log::Level severity;
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

class ParseResult;

class Reader
{
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
    ServerReaderHandler(const QnUuid& moduleId, QnResourcePool* resPool);

    virtual QnUuid moduleGuid() const override;
    virtual QnUuid archiveCamTypeId() const override;
    virtual bool isCameraInResPool(const QnUuid& cameraId) const override;
    virtual void handleError(const ReaderErrorInfo& errorInfo) const override;

private:
    mutable QnUuid m_archiveCamTypeId;
    QnUuid m_moduleId;
    QnResourcePool* m_resPool;
};

}
}
