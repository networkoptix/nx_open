#ifndef __DUAL_QUALITY_HELPER__
#define __DUAL_QUALITY_HELPER__

#include <nx/utils/uuid.h>

#include "recorder/device_file_catalog.h"
#include "core/resource/resource_fwd.h"
#include "nx/streaming/media_data_packet.h"
#include <server/server_globals.h>

#include <stack>

class QnStorageManager;

class QnDualQualityHelper
{
public:
    QnDualQualityHelper(
        QnStorageManager* normalStorageManager,
        QnStorageManager* backupStorageManager);

    void setResource(const QnNetworkResourcePtr &netResource);
    void openCamera(const QString& cameraUniqueId);
    void setPrefferedQuality(MediaQuality quality);
    void findDataForTime(
        const qint64                                time,
        DeviceFileCatalog::TruncableChunk           &resultChunk,
        DeviceFileCatalogPtr                        &resultCatalog,
        DeviceFileCatalog::FindMethod               findMethod,
        bool                                        preciseFind,
        const DeviceFileCatalog::UniqueChunkCont    &ignoreChunks
    );

private:
    typedef std::stack<DeviceFileCatalogPtr> SearchStack;

private:
    void findDataForTimeHelper(
        const qint64                                time,
        DeviceFileCatalog::TruncableChunk           &resultChunk,
        DeviceFileCatalogPtr                        &resultCatalog,
        DeviceFileCatalog::FindMethod               findMethod,
        bool                                        preciseFind,
        SearchStack                                 &searchStack,
        qint64                                      previousDistance,
        const DeviceFileCatalog::UniqueChunkCont    &ignoreChunks
    );

    static int64_t calcDistanceHelper(
        const DeviceFileCatalog::Chunk  &chunk,
        const int64_t                   time,
        DeviceFileCatalog::FindMethod   findMethod
    );

    typedef std::map<QnServer::StoragePool, DeviceFileCatalogPtr> PoolToCatalogMap;
private:
    PoolToCatalogMap m_catalogHi;
    PoolToCatalogMap m_catalogLow;
    MediaQuality m_quality;
    bool m_alreadyOnAltChunk;
    QnStorageManager* m_normalStorageManager = nullptr;
    QnStorageManager* m_backupStorageManager = nullptr;
};

#endif // __DUAL_QUALITY_HELPER__
