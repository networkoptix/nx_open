#ifndef __DUAL_QUALITY_HELPER__
#define __DUAL_QUALITY_HELPER__

#include <utils/common/uuid.h>

#include "recorder/device_file_catalog.h"
#include "core/resource/resource_fwd.h"
#include "core/datapacket/media_data_packet.h"
#include <server/server_globals.h>

class QnDualQualityHelper
{
public:
    QnDualQualityHelper();

    void setResource(const QnNetworkResourcePtr &netResource);
    void openCamera(const QString& cameraUniqueId);
    void setPrefferedQuality(MediaQuality quality);
    void findDataForTime(
        const qint64                    time, 
        DeviceFileCatalog::Chunk        &resultChunk, 
        DeviceFileCatalogPtr            &resultCatalog, 
        DeviceFileCatalog::FindMethod   findMethod, 
        bool                            preciseFind
    );
    //void findNextChunk(const DeviceFileCatalogPtr& currentCatalog, const DeviceFileCatalog::Chunk& currentChunk, DeviceFileCatalog::Chunk& nextChunk, DeviceFileCatalogPtr& nextCatalog);

private:
    void findDataForTimeHelper(
        const qint64                    time,
        DeviceFileCatalog::Chunk        &resultChunk,
        DeviceFileCatalogPtr            &resultCatalog,
        DeviceFileCatalog::FindMethod   findMethod,
        bool                            preciseFind,
        QnServer::StoragePool           storageManager
    );

    static int64_t calcDistanceHelper(
        const DeviceFileCatalog::Chunk  &chunk,
        const int64_t                   time,
        DeviceFileCatalog::FindMethod   findMethod
    );

private:
    DeviceFileCatalogPtr m_catalogHi[2];
    DeviceFileCatalogPtr m_catalogLow[2];
    MediaQuality m_quality;
    bool m_alreadyOnAltChunk;
};

#endif // __DUAL_QUALITY_HELPER__
